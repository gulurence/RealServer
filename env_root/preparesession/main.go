/*****************************************************************
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: main.go
      Purpose:   预建压测账号会话池

      直连 Redis Cluster 各主节点，先 CLUSTER SLOTS 拿槽位区间再按 CRC16 路由 SET
      写 session:pool{accountId}=accountId 键，绕过 WebPlatform HTTP 登录瓶颈
      client 类型手写 Redis RESP 协议，按主节点地址维护持久连接复用
  *****************************************************************/

// preparesession — 预建压测账号会话池 (session:pool{accountId} = accountId)。
// 直连 Redis Cluster 各主节点, 按 CRC16 槽位路由 SET, 绕过 WebPlatform HTTP 登录瓶颈。
package main

import (
	"bufio"
	"flag"
	"fmt"
	"io"
	"net"
	"os"
	"strconv"
	"strings"
)

type client struct {
	r *bufio.Reader
	w io.Writer
}

func (c *client) cmd(args ...string) (interface{}, error) {
	var sb strings.Builder
	fmt.Fprintf(&sb, "*%d\r\n", len(args))
	for _, a := range args {
		fmt.Fprintf(&sb, "$%d\r\n%s\r\n", len(a), a)
	}
	if _, err := c.w.Write([]byte(sb.String())); err != nil {
		return nil, err
	}
	return c.read()
}

func (c *client) read() (interface{}, error) {
	line, err := c.r.ReadString('\n')
	if err != nil {
		return nil, err
	}
	line = strings.TrimRight(line, "\r\n")
	if line == "" {
		return nil, io.EOF
	}
	switch line[0] {
	case '+':
		return line[1:], nil
	case '-':
		return nil, fmt.Errorf("redis: %s", line[1:])
	case ':':
		return strconv.ParseInt(line[1:], 10, 64)
	case '$':
		n, _ := strconv.Atoi(line[1:])
		if n < 0 {
			return nil, nil
		}
		buf := make([]byte, n+2)
		if _, err := io.ReadFull(c.r, buf); err != nil {
			return nil, err
		}
		return string(buf[:n]), nil
	case '*':
		n, _ := strconv.Atoi(line[1:])
		if n < 0 {
			return nil, nil
		}
		arr := make([]interface{}, 0, n)
		for i := 0; i < n; i++ {
			v, err := c.read()
			if err != nil {
				return nil, err
			}
			arr = append(arr, v)
		}
		return arr, nil
	}
	return nil, fmt.Errorf("bad resp line %q", line)
}

// crc16 — Redis Cluster 键槽位算法 (CRC16-CCITT, poly 0x1021)。
func crc16(s string) uint16 {
	var crc uint16 = 0
	for i := 0; i < len(s); i++ {
		crc ^= uint16(s[i]) << 8
		for j := 0; j < 8; j++ {
			if crc&0x8000 != 0 {
				crc = (crc << 1) ^ 0x1021
			} else {
				crc <<= 1
			}
		}
	}
	return crc
}

func slot(key string) uint16 {
	return crc16(key) % 16384
}

func main() {
	base := flag.Int64("pool-base", 1000000, "起始 account_id (含)")
	count := flag.Int64("count", 1, "账号数量")
	flag.Parse()

	// 连 7000 拿 CLUSTER SLOTS → 槽位区间 → 主节点地址映射
	conn, err := net.Dial("tcp", "127.0.0.1:7000")
	if err != nil {
		fmt.Fprintf(os.Stderr, "dial 7000: %v\n", err)
		os.Exit(1)
	}
	c0 := &client{bufio.NewReader(conn), conn}
	slotsRaw, err := c0.cmd("CLUSTER", "SLOTS")
	if err != nil {
		fmt.Fprintf(os.Stderr, "CLUSTER SLOTS: %v\n", err)
		os.Exit(1)
	}
	conn.Close()

	// slotsRaw: []interface{ 每个槽段: [start, end, [ip, port, ...], ...] }
	type slotRange struct {
		start, end int
		host       string
		port       int
	}
	var ranges []slotRange
	for _, segI := range slotsRaw.([]interface{}) {
		seg := segI.([]interface{})
		start, _ := strconv.Atoi(fmt.Sprint(seg[0]))
		end, _ := strconv.Atoi(fmt.Sprint(seg[1]))
		master := seg[2].([]interface{})
		host := fmt.Sprint(master[0])
		port, _ := strconv.Atoi(fmt.Sprint(master[1]))
		ranges = append(ranges, slotRange{start, end, host, port})
	}

	// 每个主节点一个持久连接
	conns := map[string]*client{}
	getConn := func(host string, port int) (*client, error) {
		key := fmt.Sprintf("%s:%d", host, port)
		if c, ok := conns[key]; ok {
			return c, nil
		}
		c, err := net.Dial("tcp", key)
		if err != nil {
			return nil, err
		}
		cl := &client{bufio.NewReader(c), c}
		conns[key] = cl
		return cl, nil
	}
	defer func() {
		for _, c := range conns {
			if cc, ok := c.w.(net.Conn); ok {
				cc.Close()
			}
		}
	}()

	total := 0
	for i := int64(1); i <= *count; i++ {
		accountID := *base + i
		token := fmt.Sprintf("pool%d", accountID)
		key := "session:" + token
		s := slot(key)
		var target *slotRange
		for ri := range ranges {
			if int(s) >= ranges[ri].start && int(s) <= ranges[ri].end {
				target = &ranges[ri]
				break
			}
		}
		if target == nil {
			fmt.Fprintf(os.Stderr, "no slot for %s (slot=%d)\n", key, s)
			continue
		}
		c, err := getConn(target.host, target.port)
		if err != nil {
			fmt.Fprintf(os.Stderr, "connect %s:%d: %v\n", target.host, target.port, err)
			continue
		}
		if _, err := c.cmd("SET", key, strconv.FormatInt(accountID, 10)); err != nil {
			fmt.Fprintf(os.Stderr, "SET %s: %v\n", key, err)
			continue
		}
		total++
	}
	fmt.Printf("preparesession: wrote %d session keys (base=%d count=%d)\n", total, *base, *count)
}
