/*****************************************************************
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: flow.go
      Purpose:   单个虚拟客户端完整链路流程实现

      runClient 实现连 Gate、登录、创角、选角后按模式在线驻留或进入战斗
      runBattle 实现建房、准备、开局、直连 Battle 网关并逐帧上报输入
      提供 sendFrame、recvFrame、recvMatching 帧 IO 与 result 结构记录耗时及收发帧数
  *****************************************************************/

package main

// 单个虚拟客户端的完整链路: 连 Gate → 登录 → 创角 → 选角 → (在线驻留 | 建房开战帧同步)。

import (
	"encoding/binary"
	"fmt"
	"io"
	"net"
	"sync/atomic"
	"time"
)

// ---------------------------------------------------------------------------
// 帧 IO: [uint32 LE bodyLen][protobuf body]
// ---------------------------------------------------------------------------
func sendFrame(conn net.Conn, body []byte) error {
	var lenBuf [4]byte
	binary.LittleEndian.PutUint32(lenBuf[:], uint32(len(body)))
	if _, err := conn.Write(lenBuf[:]); err != nil {
		return err
	}
	_, err := conn.Write(body)
	return err
}

func recvFrame(conn net.Conn) ([]byte, error) {
	var lenBuf [4]byte
	if _, err := io.ReadFull(conn, lenBuf[:]); err != nil {
		return nil, err
	}
	n := binary.LittleEndian.Uint32(lenBuf[:])
	if n > 64*1024*1024 { // 防御性上限
		return nil, fmt.Errorf("oversize frame %d", n)
	}
	body := make([]byte, n)
	if _, err := io.ReadFull(conn, body); err != nil {
		return nil, err
	}
	return body, nil
}

// recvMatching 读帧直到命中 (big, small), 中间插入的 NTF/广播直接丢弃。
func recvMatching(conn net.Conn, big, small uint32) ([]byte, error) {
	for {
		body, err := recvFrame(conn)
		if err != nil {
			return nil, err
		}
		rb, rs := parseRoute(body)
		if rb == big && rs == small {
			return body, nil
		}
	}
}

// ---------------------------------------------------------------------------
// 结果
// ---------------------------------------------------------------------------
type result struct {
	index      int
	ok         bool
	errCode    int    // 业务错误码 (0=无)
	errMsg     string // 网络/解析错误描述
	latencyMs  int64  // 全链路耗时
	framesSent int
	framesRcvd int64
	battleID   int64
	roomID     int64
}

func (r result) failReason() string {
	if r.errCode != 0 {
		return fmt.Sprintf("err=%d", r.errCode)
	}
	return r.errMsg
}

// ---------------------------------------------------------------------------
// 单客户端流程
// ---------------------------------------------------------------------------
func runClient(id int, a *args, res chan<- result) {
	r := result{index: id}
	start := time.Now()

	accountID := a.poolBase + int64(id)
	token := fmt.Sprintf("pool%d", accountID)
	roleName := fmt.Sprintf("s%d", accountID) // 全局唯一 (随 poolBase 变化)

	gate, err := net.DialTimeout("tcp", fmt.Sprintf("%s:%d", a.gateHost, a.gatePort), 10*time.Second)
	if err != nil {
		r.errMsg = "dial gate: " + err.Error()
		res <- r
		return
	}
	defer gate.Close()
	gate.SetDeadline(time.Now().Add(a.flowTimeout))

	// 1. 登录
	if err := sendFrame(gate, buildLoginReq(accountID, token)); err != nil {
		r.errMsg = "send login: " + err.Error()
		res <- r
		return
	}
	body, err := recvMatching(gate, AT_LOGIN, loginRsp)
	if err != nil {
		r.errMsg = "login rsp: " + err.Error()
		res <- r
		return
	}
	if ec, err := rspErrCode(body); err != nil {
		r.errMsg = "parse login rsp: " + err.Error()
		res <- r
		return
	} else if ec != ERR_NONE {
		r.errCode = int(ec)
		res <- r
		return
	}

	// 2. 创角
	if err := sendFrame(gate, buildCreateRoleReq(accountID, roleName)); err != nil {
		r.errMsg = "send create role: " + err.Error()
		res <- r
		return
	}
	body, err = recvMatching(gate, AT_LOGIN, createRoleRsp)
	if err != nil {
		r.errMsg = "create role rsp: " + err.Error()
		res <- r
		return
	}
	fields, err := parseWire(body)
	if err != nil {
		r.errMsg = "parse create role rsp: " + err.Error()
		res <- r
		return
	}
	if ec, _ := fieldVarint(fields, 3); ec != ERR_NONE {
		r.errCode = int(ec)
		res <- r
		return
	}
	// role 子消息 (field 4) → role_id (field 1)
	var roleID int64
	if roleBytes, ok := fieldBytes(fields, 4); ok {
		if rfs, err := parseWire(roleBytes); err == nil {
			if v, ok := fieldVarint(rfs, 1); ok {
				roleID = int64(v)
			}
		}
	}
	if roleID == 0 {
		r.errMsg = "empty role_id"
		res <- r
		return
	}

	// 3. 选角
	if err := sendFrame(gate, buildSelectRoleReq(accountID, roleID)); err != nil {
		r.errMsg = "send select role: " + err.Error()
		res <- r
		return
	}
	body, err = recvMatching(gate, AT_LOGIN, selectRoleRsp)
	if err != nil {
		r.errMsg = "select role rsp: " + err.Error()
		res <- r
		return
	}
	if ec, err := rspErrCode(body); err != nil {
		r.errMsg = "parse select role rsp: " + err.Error()
		res <- r
		return
	} else if ec != ERR_NONE {
		r.errCode = int(ec)
		res <- r
		return
	}

	// 在线模式: 选角成功后驻留 --hold, 再退出 (保持 Player actor 在线)
	if a.mode == "login" {
		if a.hold > 0 {
			time.Sleep(a.hold)
		}
		r.ok = true
		r.latencyMs = time.Since(start).Milliseconds()
		res <- r
		return
	}

	// 战斗模式
	runBattle(a, gate, roleID, &r, res, start)
}

// rspErrCode 解析响应并返回 error_code (field 3)。
func rspErrCode(body []byte) (uint64, error) {
	fields, err := parseWire(body)
	if err != nil {
		return 0, err
	}
	ec, _ := fieldVarint(fields, 3)
	return ec, nil
}

// runBattle: 建房 → 准备 → 开局 → 直连 Battle 网关 → 帧同步。
func runBattle(a *args, gate net.Conn, roleID int64, r *result, res chan<- result, start time.Time) {
	// 4. 建房
	if err := sendFrame(gate, buildRoomCreateReq()); err != nil {
		r.errMsg = "create room: " + err.Error()
		res <- *r
		return
	}
	body, err := recvMatching(gate, AT_ROOM, roomCreateRsp)
	if err != nil {
		r.errMsg = "create room rsp: " + err.Error()
		res <- *r
		return
	}
	fields, err := parseWire(body)
	if err != nil {
		r.errMsg = "parse create room rsp: " + err.Error()
		res <- *r
		return
	}
	if ec, _ := fieldVarint(fields, 3); ec != ERR_NONE {
		r.errCode = int(ec)
		res <- *r
		return
	}
	roomID, _ := fieldVarint(fields, 4)
	r.roomID = int64(roomID)

	// 5. 准备
	if err := sendFrame(gate, buildRoomReadyReq(r.roomID)); err != nil {
		r.errMsg = "ready: " + err.Error()
		res <- *r
		return
	}
	body, err = recvMatching(gate, AT_ROOM, roomReadyRsp)
	if err != nil {
		r.errMsg = "ready rsp: " + err.Error()
		res <- *r
		return
	}
	if ec, err := rspErrCode(body); err != nil {
		r.errMsg = "parse ready rsp: " + err.Error()
		res <- *r
		return
	} else if ec != ERR_NONE {
		r.errCode = int(ec)
		res <- *r
		return
	}

	// 6. 开局
	if err := sendFrame(gate, buildRoomStartReq(r.roomID)); err != nil {
		r.errMsg = "start: " + err.Error()
		res <- *r
		return
	}
	body, err = recvMatching(gate, AT_ROOM, roomStartRsp)
	if err != nil {
		r.errMsg = "start rsp: " + err.Error()
		res <- *r
		return
	}
	fields, err = parseWire(body)
	if err != nil {
		r.errMsg = "parse start rsp: " + err.Error()
		res <- *r
		return
	}
	if ec, _ := fieldVarint(fields, 3); ec != ERR_NONE {
		r.errCode = int(ec)
		res <- *r
		return
	}
	battleID, _ := fieldVarint(fields, 5)
	r.battleID = int64(battleID)
	authKey, _ := fieldString(fields, 6)
	host, _ := fieldString(fields, 7)
	port, _ := fieldVarint(fields, 8)
	if host == "" || host == "0.0.0.0" {
		host = "127.0.0.1"
	}

	// 7. 直连 Battle 网关
	bsock, err := net.DialTimeout("tcp", fmt.Sprintf("%s:%d", host, port), 10*time.Second)
	if err != nil {
		r.errMsg = "dial battle: " + err.Error()
		res <- *r
		return
	}
	defer bsock.Close()
	bsock.SetDeadline(time.Now().Add(a.flowTimeout))

	if err := sendFrame(bsock, buildBattleLoginReq(r.battleID, authKey, roleID)); err != nil {
		r.errMsg = "battle login: " + err.Error()
		res <- *r
		return
	}
	body, err = recvMatching(bsock, AT_BATTLE, battleLoginRsp)
	if err != nil {
		r.errMsg = "battle login rsp: " + err.Error()
		res <- *r
		return
	}
	if ec, err := rspErrCode(body); err != nil {
		r.errMsg = "parse battle login rsp: " + err.Error()
		res <- *r
		return
	} else if ec != ERR_NONE {
		r.errCode = int(ec)
		res <- *r
		return
	}

	if err := sendFrame(bsock, buildBattleEnterReq(r.battleID, authKey, roleID)); err != nil {
		r.errMsg = "battle enter: " + err.Error()
		res <- *r
		return
	}
	body, err = recvMatching(bsock, AT_BATTLE, battleEnterRsp)
	if err != nil {
		r.errMsg = "battle enter rsp: " + err.Error()
		res <- *r
		return
	}
	if ec, err := rspErrCode(body); err != nil {
		r.errMsg = "parse battle enter rsp: " + err.Error()
		res <- *r
		return
	} else if ec != ERR_NONE {
		r.errCode = int(ec)
		res <- *r
		return
	}

	// 8. 帧同步: 后台协程排空服务器回推 (FRAME_NTF/START_NTF), 前台逐帧上报输入。
	var rcvd int64
	readerDone := make(chan struct{})
	go func() {
		defer close(readerDone)
		for {
			if _, err := recvFrame(bsock); err != nil {
				return
			}
			atomic.AddInt64(&rcvd, 1)
		}
	}()

	for f := uint32(1); f <= a.battleFrames; f++ {
		if err := sendFrame(bsock, buildBattleInputReq(r.battleID, f, roleID)); err != nil {
			break
		}
		r.framesSent++
		if a.frameInterval > 0 {
			time.Sleep(a.frameInterval)
		}
	}
	r.framesRcvd = atomic.LoadInt64(&rcvd)
	bsock.Close()
	<-readerDone

	r.ok = true
	r.latencyMs = time.Since(start).Milliseconds()
	res <- *r
}
