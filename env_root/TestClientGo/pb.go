/*****************************************************************
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: pb.go
      Purpose:   极简 protobuf wire 格式编解码与消息构造

      手写 varint 编解码与 parseWire，parseRoute 只读 field1、field2 做 big_type、small_type 快速路由
      提供 buildLoginReq、buildCreateRoleReq、buildSelectRoleReq、buildRoom 系列与 buildBattle 系列消息构造
      不引入 google.golang.org/protobuf，保持零外部依赖，消息常量与 msg_id.proto 对齐
  *****************************************************************/

package main

// 极简 protobuf wire-format 编解码 (仅够本压测客户端的消息)。
// 不引入 google.golang.org/protobuf: 消息全部是 varint/string/内嵌子消息,
// 手写 wire 编解码即可, 保持零外部依赖。

import "fmt"

// ---------------------------------------------------------------------------
// 消息路由常量 (与 msg_id.proto / login/room/battle.proto 一致)
// ---------------------------------------------------------------------------
const (
	AT_LOGIN  = 101
	AT_ROOM   = 103
	AT_BATTLE = 203
	ERR_NONE  = 0
)

// login small_type
const (
	loginReq      = 1
	loginRsp      = 2
	selectRoleReq = 3
	selectRoleRsp = 4
	createRoleReq = 5
	createRoleRsp = 6
)

// room small_type
const (
	roomCreateReq = 1
	roomCreateRsp = 2
	roomReadyReq  = 9
	roomReadyRsp  = 10
	roomStartReq  = 14
	roomStartRsp  = 15
)

// battle small_type
const (
	battleEnterReq = 1
	battleEnterRsp = 2
	battleInputReq = 4
	battleFrameNtf = 5
	battleLoginReq = 12
	battleLoginRsp = 13
)

// ---------------------------------------------------------------------------
// varint
// ---------------------------------------------------------------------------
func consumeVarint(b []byte) (uint64, int) {
	var v uint64
	for i := 0; i < len(b) && i < 10; i++ {
		c := b[i]
		v |= uint64(c&0x7f) << (7 * i)
		if c&0x80 == 0 {
			return v, i + 1
		}
	}
	return 0, 0
}

func appendVarint(b []byte, v uint64) []byte {
	for v >= 0x80 {
		b = append(b, byte(v)|0x80)
		v >>= 7
	}
	return append(b, byte(v))
}

// ---------------------------------------------------------------------------
// 编码
// ---------------------------------------------------------------------------
func appendUvarintField(b []byte, fieldNum int, v uint64) []byte {
	b = appendVarint(b, uint64(fieldNum<<3|0))
	return appendVarint(b, v)
}

func appendInt64Field(b []byte, fieldNum int, v int64) []byte {
	return appendUvarintField(b, fieldNum, uint64(v))
}

func appendBoolField(b []byte, fieldNum int, v bool) []byte {
	if v {
		return appendUvarintField(b, fieldNum, 1)
	}
	return b // proto3 默认 false 省略
}

func appendBytesField(b []byte, fieldNum int, data []byte) []byte {
	b = appendVarint(b, uint64(fieldNum<<3|2))
	b = appendVarint(b, uint64(len(data)))
	return append(b, data...)
}

func appendStringField(b []byte, fieldNum int, s string) []byte {
	return appendBytesField(b, fieldNum, []byte(s))
}

// ---------------------------------------------------------------------------
// 解码
// ---------------------------------------------------------------------------
type wireField struct {
	num    int
	wire   int
	varint uint64
	bytes  []byte
}

func parseWire(data []byte) ([]wireField, error) {
	fields := make([]wireField, 0, 8)
	for i := 0; i < len(data); {
		tag, n := consumeVarint(data[i:])
		if n == 0 {
			return nil, fmt.Errorf("bad tag at %d", i)
		}
		i += n
		num := int(tag >> 3)
		wt := int(tag & 7)
		switch wt {
		case 0:
			v, n2 := consumeVarint(data[i:])
			if n2 == 0 {
				return nil, fmt.Errorf("bad varint at %d", i)
			}
			i += n2
			fields = append(fields, wireField{num, wt, v, nil})
		case 2:
			l, n2 := consumeVarint(data[i:])
			if n2 == 0 {
				return nil, fmt.Errorf("bad len at %d", i)
			}
			i += n2
			if i+int(l) > len(data) {
				return nil, fmt.Errorf("truncated at %d", i)
			}
			fields = append(fields, wireField{num, wt, 0, data[i : i+int(l)]})
			i += int(l)
		case 5: // fixed32
			if i+4 > len(data) {
				return nil, fmt.Errorf("truncated fixed32 at %d", i)
			}
			i += 4
		case 1: // fixed64
			if i+8 > len(data) {
				return nil, fmt.Errorf("truncated fixed64 at %d", i)
			}
			i += 8
		default:
			return nil, fmt.Errorf("bad wire type %d", wt)
		}
	}
	return fields, nil
}

func fieldVarint(fields []wireField, num int) (uint64, bool) {
	for _, f := range fields {
		if f.num == num && f.wire == 0 {
			return f.varint, true
		}
	}
	return 0, false
}

func fieldBytes(fields []wireField, num int) ([]byte, bool) {
	for _, f := range fields {
		if f.num == num && f.wire == 2 {
			return f.bytes, true
		}
	}
	return nil, false
}

func fieldString(fields []wireField, num int) (string, bool) {
	b, ok := fieldBytes(fields, num)
	if !ok {
		return "", false
	}
	return string(b), true
}

// parseRoute 只读 field 1/2 (big_type/small_type) 两个 varint, 快速路由判定。
func parseRoute(data []byte) (big, small uint32) {
	for i := 0; i < len(data); {
		tag, n := consumeVarint(data[i:])
		if n == 0 {
			return
		}
		i += n
		num := int(tag >> 3)
		wt := int(tag & 7)
		switch wt {
		case 0:
			v, n2 := consumeVarint(data[i:])
			if n2 == 0 {
				return
			}
			i += n2
			if num == 1 {
				big = uint32(v)
			} else if num == 2 {
				small = uint32(v)
			}
		case 2:
			l, n2 := consumeVarint(data[i:])
			if n2 == 0 {
				return
			}
			i += n2 + int(l)
		case 5:
			i += 4
		case 1:
			i += 8
		default:
			return
		}
	}
	return
}

// ---------------------------------------------------------------------------
// 各消息构造
// ---------------------------------------------------------------------------
func buildLoginReq(accountID int64, token string) []byte {
	b := []byte{}
	b = appendUvarintField(b, 1, AT_LOGIN)
	b = appendUvarintField(b, 2, loginReq)
	b = appendStringField(b, 3, token)
	b = appendInt64Field(b, 4, accountID)
	return b
}

func buildCreateRoleReq(accountID int64, roleName string) []byte {
	b := []byte{}
	b = appendUvarintField(b, 1, AT_LOGIN)
	b = appendUvarintField(b, 2, createRoleReq)
	b = appendInt64Field(b, 3, accountID)
	b = appendStringField(b, 4, roleName)
	b = appendUvarintField(b, 5, 1) // job
	return b
}

func buildSelectRoleReq(accountID, roleID int64) []byte {
	b := []byte{}
	b = appendUvarintField(b, 1, AT_LOGIN)
	b = appendUvarintField(b, 2, selectRoleReq)
	b = appendInt64Field(b, 3, accountID)
	b = appendInt64Field(b, 4, roleID)
	return b
}

func buildRoomCreateReq() []byte {
	b := []byte{}
	b = appendUvarintField(b, 1, AT_ROOM)
	b = appendUvarintField(b, 2, roomCreateReq)
	b = appendUvarintField(b, 3, 1) // mode = PVE
	b = appendUvarintField(b, 4, 1) // visibility = PUBLIC
	b = appendUvarintField(b, 6, 5) // max_members = 5
	b = appendUvarintField(b, 7, 11) // level_id = 11
	// difficulty=8 默认 0 省略
	return b
}

func buildRoomReadyReq(roomID int64) []byte {
	b := []byte{}
	b = appendUvarintField(b, 1, AT_ROOM)
	b = appendUvarintField(b, 2, roomReadyReq)
	b = appendInt64Field(b, 3, roomID)
	b = appendBoolField(b, 4, true)
	return b
}

func buildRoomStartReq(roomID int64) []byte {
	b := []byte{}
	b = appendUvarintField(b, 1, AT_ROOM)
	b = appendUvarintField(b, 2, roomStartReq)
	b = appendInt64Field(b, 3, roomID)
	return b
}

func buildBattleLoginReq(battleID int64, authKey string, playerID int64) []byte {
	b := []byte{}
	b = appendUvarintField(b, 1, AT_BATTLE)
	b = appendUvarintField(b, 2, battleLoginReq)
	b = appendInt64Field(b, 3, battleID)
	b = appendStringField(b, 4, authKey)
	b = appendInt64Field(b, 5, playerID)
	// spectate=6 默认 false 省略
	return b
}

func buildBattleEnterReq(battleID int64, authKey string, playerID int64) []byte {
	b := []byte{}
	b = appendUvarintField(b, 1, AT_BATTLE)
	b = appendUvarintField(b, 2, battleEnterReq)
	b = appendInt64Field(b, 3, battleID)
	b = appendStringField(b, 4, authKey)
	b = appendInt64Field(b, 5, playerID)
	return b
}

func buildBattleInputReq(battleID int64, frame uint32, playerID int64) []byte {
	in := []byte{}
	in = appendInt64Field(in, 1, playerID) // input.player_id
	in = appendUvarintField(in, 2, 1)      // input.move_dir = 1

	b := []byte{}
	b = appendUvarintField(b, 1, AT_BATTLE)
	b = appendUvarintField(b, 2, battleInputReq)
	b = appendInt64Field(b, 3, battleID)
	b = appendUvarintField(b, 4, uint64(frame)) // client_frame
	b = appendBytesField(b, 5, in)              // input
	return b
}
