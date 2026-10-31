/*****************************************************************
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: main.go
      Purpose:   逐节点清理 Redis Cluster 测试态键

      直连本机 7000 至 7005 六个节点，按 room:*、battle:*、cluster:actor:*、session:* 等模式 KEYS 后 DEL
      client 类型手写 Redis RESP 协议编解码，每个节点一条持久连接
      清理后打印 cluster:load:player_count 现值，供压测后重置测试环境
  *****************************************************************/

// cleanredis — 逐节点清理 Redis Cluster 测试态键 (KEYS + DEL, 持久连接)。
// 仅清测试态: room:* battle:* cluster:actor:* cluster:load:* session:pool*。
package main

import (
	"bufio"
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
		arr := make([]string, 0, n)
		for i := 0; i < n; i++ {
			v, err := c.read()
			if err != nil {
				return nil, err
			}
			s, _ := v.(string)
			arr = append(arr, s)
		}
		return arr, nil
	}
	return nil, fmt.Errorf("bad resp line %q", line)
}

func main() {
	nodes := []int{7000, 7001, 7002, 7003, 7004, 7005}
	patterns := []string{
		"room:*", "battle:*", "cluster:actor:*", "cluster:load:*",
		"session:*", "account:*", "rolename:*", "player:*",
		"actor:index:*", "actor:state:*", "cluster:process:*",
		"cluster:migrate:*",
	}

	total := 0
	for _, port := range nodes {
		conn, err := net.Dial("tcp", fmt.Sprintf("127.0.0.1:%d", port))
		if err != nil {
			fmt.Fprintf(os.Stderr, "dial %d: %v\n", port, err)
			continue
		}
		c := &client{bufio.NewReader(conn), conn}
		for _, pat := range patterns {
			v, err := c.cmd("KEYS", pat)
			if err != nil {
				continue
			}
			keys, _ := v.([]string)
			for _, k := range keys {
				if _, err := c.cmd("DEL", k); err == nil {
					total++
				}
			}
		}
		conn.Close()
	}
	fmt.Printf("cleanredis: deleted %d keys\n", total)

	// 打印 player_count 现值
	conn, err := net.Dial("tcp", "127.0.0.1:7000")
	if err == nil {
		c := &client{bufio.NewReader(conn), conn}
		if v, err := c.cmd("GET", "cluster:load:player_count"); err == nil {
			fmt.Printf("cluster:load:player_count = %v\n", v)
		}
		conn.Close()
	}
}
