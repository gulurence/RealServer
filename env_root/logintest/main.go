package main

// logintest — 验证登录「顶号 / 登录中超时判定」三种场景 (自包含, 复用 TestClientGo 的 wire 格式):
//   1. IN_GAME 顶号: 已进游戏的账号二次登录 → 成功并踢旧连接 (旧连接被服务器断开)
//   2. 登录中未超时: LOGGED_IN 状态下二次登录 → 返回登录失败 (ERR_SESSION_INVALID)
//   3. 登录中超时:   LOGGED_IN 超过 15s 后二次登录 → 成功替换
// 需先 preparesession 预写 session 键; 依赖 cluster 已启动。

import (
	"encoding/binary"
	"flag"
	"fmt"
	"io"
	"net"
	"os"
	"time"
)

const (
	atLogin          = 101
	loginReq         = 1
	loginRsp         = 2
	selectRoleReq    = 3
	selectRoleRsp    = 4
	createRoleReq    = 5
	createRoleRsp    = 6
	errNone          = 0
	errSessionInv    = 1001
)

type wf struct {
	num    int
	wire   int
	varint uint64
	bytes  []byte
}

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

func appendUvarintField(b []byte, n int, v uint64) []byte {
	b = appendVarint(b, uint64(n<<3|0))
	return appendVarint(b, v)
}

func appendInt64Field(b []byte, n int, v int64) []byte {
	return appendUvarintField(b, n, uint64(v))
}

func appendStringField(b []byte, n int, s string) []byte {
	b = appendVarint(b, uint64(n<<3|2))
	b = appendVarint(b, uint64(len(s)))
	return append(b, s...)
}

func parseWire(data []byte) ([]wf, error) {
	var fs []wf
	for i := 0; i < len(data); {
		tag, n := consumeVarint(data[i:])
		if n == 0 {
			return nil, fmt.Errorf("bad tag")
		}
		i += n
		num, wt := int(tag>>3), int(tag&7)
		switch wt {
		case 0:
			v, n2 := consumeVarint(data[i:])
			if n2 == 0 {
				return nil, fmt.Errorf("bad varint")
			}
			i += n2
			fs = append(fs, wf{num, wt, v, nil})
		case 2:
			l, n2 := consumeVarint(data[i:])
			if n2 == 0 {
				return nil, fmt.Errorf("bad len")
			}
			i += n2
			if i+int(l) > len(data) {
				return nil, fmt.Errorf("truncated")
			}
			fs = append(fs, wf{num, wt, 0, data[i : i+int(l)]})
			i += int(l)
		case 5:
			i += 4
		case 1:
			i += 8
		default:
			return nil, fmt.Errorf("bad wire type")
		}
	}
	return fs, nil
}

func fVarint(fs []wf, n int) (uint64, bool) {
	for _, f := range fs {
		if f.num == n && f.wire == 0 {
			return f.varint, true
		}
	}
	return 0, false
}

func fBytes(fs []wf, n int) ([]byte, bool) {
	for _, f := range fs {
		if f.num == n && f.wire == 2 {
			return f.bytes, true
		}
	}
	return nil, false
}

func sendFrame(c net.Conn, body []byte) error {
	var lb [4]byte
	binary.LittleEndian.PutUint32(lb[:], uint32(len(body)))
	if _, err := c.Write(lb[:]); err != nil {
		return err
	}
	_, err := c.Write(body)
	return err
}

func recvFrame(c net.Conn) ([]byte, error) {
	var lb [4]byte
	if _, err := io.ReadFull(c, lb[:]); err != nil {
		return nil, err
	}
	n := binary.LittleEndian.Uint32(lb[:])
	if n > 64*1024*1024 {
		return nil, fmt.Errorf("oversize")
	}
	body := make([]byte, n)
	if _, err := io.ReadFull(c, body); err != nil {
		return nil, err
	}
	return body, nil
}

func parseRoute(data []byte) (big, small uint32) {
	for i := 0; i < len(data); {
		tag, n := consumeVarint(data[i:])
		if n == 0 {
			return
		}
		i += n
		num, wt := int(tag>>3), int(tag&7)
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

func recvMatching(c net.Conn, big, small uint32) ([]byte, error) {
	for {
		body, err := recvFrame(c)
		if err != nil {
			return nil, err
		}
		b, s := parseRoute(body)
		if b == big && s == small {
			return body, nil
		}
	}
}

func buildLoginReq(accountID int64, token string) []byte {
	b := []byte{}
	b = appendUvarintField(b, 1, atLogin)
	b = appendUvarintField(b, 2, loginReq)
	b = appendStringField(b, 3, token)
	b = appendInt64Field(b, 4, accountID)
	return b
}

func buildCreateRoleReq(accountID int64, roleName string) []byte {
	b := []byte{}
	b = appendUvarintField(b, 1, atLogin)
	b = appendUvarintField(b, 2, createRoleReq)
	b = appendInt64Field(b, 3, accountID)
	b = appendStringField(b, 4, roleName)
	b = appendUvarintField(b, 5, 1)
	return b
}

func buildSelectRoleReq(accountID, roleID int64) []byte {
	b := []byte{}
	b = appendUvarintField(b, 1, atLogin)
	b = appendUvarintField(b, 2, selectRoleReq)
	b = appendInt64Field(b, 3, accountID)
	b = appendInt64Field(b, 4, roleID)
	return b
}

func dialGate(addr string) (net.Conn, error) {
	return net.DialTimeout("tcp", addr, 10*time.Second)
}

func loginErrCode(body []byte) (uint64, error) {
	fs, err := parseWire(body)
	if err != nil {
		return 0, err
	}
	v, _ := fVarint(fs, 3)
	return v, nil
}

// loginOnly 发 LOGIN_REQ 并读回 LOGIN_RSP, 返回 error_code。
func loginOnly(c net.Conn, accountID int64, token string) (uint64, error) {
	if err := sendFrame(c, buildLoginReq(accountID, token)); err != nil {
		return 0, err
	}
	body, err := recvMatching(c, atLogin, loginRsp)
	if err != nil {
		return 0, err
	}
	return loginErrCode(body)
}

func main() {
	addr := flag.String("gate", "127.0.0.1:10080", "Gate 地址")
	base := flag.Int64("base", 9000000, "起始 account_id")
	flag.Parse()

	fmt.Printf("=== logintest: gate=%s base=%d ===\n", *addr, *base)
	pass, fail := 0, 0
	report := func(name string, ok bool, detail string) {
		if ok {
			pass++
			fmt.Printf("[PASS] %s — %s\n", name, detail)
		} else {
			fail++
			fmt.Printf("[FAIL] %s — %s\n", name, detail)
		}
	}

	// ---------- 场景 1: IN_GAME 顶号 ----------
	{
		id := *base + 1
		token := fmt.Sprintf("pool%d", id)
		name := "场景1 IN_GAME 顶号"
		c1, err := dialGate(*addr)
		if err != nil {
			report(name, false, "dial c1: "+err.Error())
		} else {
			defer c1.Close()
			// 登录
			if ec, err := loginOnly(c1, id, token); err != nil || ec != errNone {
				report(name, false, fmt.Sprintf("c1 登录 ec=%d err=%v", ec, err))
			} else {
				// 创角
				if err := sendFrame(c1, buildCreateRoleReq(id, fmt.Sprintf("s%d", id))); err != nil {
					report(name, false, "c1 创角发帧: "+err.Error())
				} else {
					body, err := recvMatching(c1, atLogin, createRoleRsp)
					if err != nil {
						report(name, false, "c1 创角读回: "+err.Error())
					} else {
						fs, _ := parseWire(body)
						ec, _ := fVarint(fs, 3)
						if ec != errNone {
							report(name, false, fmt.Sprintf("c1 创角 ec=%d", ec))
						} else {
							roleBytes, _ := fBytes(fs, 4)
							rfs, _ := parseWire(roleBytes)
							roleID, _ := fVarint(rfs, 1)
							// 选角 → IN_GAME
							if err := sendFrame(c1, buildSelectRoleReq(id, int64(roleID))); err != nil {
								report(name, false, "c1 选角发帧: "+err.Error())
							} else if body, err := recvMatching(c1, atLogin, selectRoleRsp); err != nil {
								report(name, false, "c1 选角读回: "+err.Error())
							} else if ec, _ := loginErrCode(body); ec != errNone {
								report(name, false, fmt.Sprintf("c1 选角 ec=%d", ec))
							} else {
								// c1 已 IN_GAME; 二次登录 (顶号)
								c2, err := dialGate(*addr)
								if err != nil {
									report(name, false, "dial c2: "+err.Error())
								} else {
									defer c2.Close()
									ec, err := loginOnly(c2, id, token)
									if err != nil {
										report(name, false, "c2 登录 err: "+err.Error())
									} else if ec != errNone {
										report(name, false, fmt.Sprintf("c2 登录应成功但 ec=%d", ec))
									} else {
										// 顶号成功: c1 应被服务器断开
										c1.SetDeadline(time.Now().Add(5 * time.Second))
										_, rerr := recvFrame(c1)
										if rerr == nil {
											report(name, false, "c1 未被踢 (仍能收帧)")
										} else {
											report(name, true, fmt.Sprintf("c2 顶号成功 ec=0, c1 被踢 (%v)", rerr))
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}

	// ---------- 场景 2: 登录中未超时 → 拒绝 ----------
	{
		id := *base + 2
		token := fmt.Sprintf("pool%d", id)
		name := "场景2 登录中未超时拒绝"
		c1, err := dialGate(*addr)
		if err != nil {
			report(name, false, "dial c1: "+err.Error())
		} else {
			defer c1.Close()
			if ec, err := loginOnly(c1, id, token); err != nil || ec != errNone {
				report(name, false, fmt.Sprintf("c1 首次登录 ec=%d err=%v", ec, err))
			} else {
				c2, err := dialGate(*addr)
				if err != nil {
					report(name, false, "dial c2: "+err.Error())
				} else {
					defer c2.Close()
					ec, err := loginOnly(c2, id, token)
					if err != nil {
						report(name, false, "c2 登录 err: "+err.Error())
					} else if ec != errSessionInv {
						report(name, false, fmt.Sprintf("c2 应被拒 (1001) 但 ec=%d", ec))
					} else {
						report(name, true, fmt.Sprintf("c2 被拒 ec=%d (登录中未超时)", ec))
					}
				}
			}
		}
	}

	// ---------- 场景 3: 登录中超时 → 替换 ----------
	{
		id := *base + 3
		token := fmt.Sprintf("pool%d", id)
		name := "场景3 登录中超时替换"
		c1, err := dialGate(*addr)
		if err != nil {
			report(name, false, "dial c1: "+err.Error())
		} else {
			defer c1.Close()
			if ec, err := loginOnly(c1, id, token); err != nil || ec != errNone {
				report(name, false, fmt.Sprintf("c1 首次登录 ec=%d err=%v", ec, err))
			} else {
				fmt.Printf("       (等待 16s 让登录超时...)\n")
				time.Sleep(16 * time.Second)
				c2, err := dialGate(*addr)
				if err != nil {
					report(name, false, "dial c2: "+err.Error())
				} else {
					defer c2.Close()
					ec, err := loginOnly(c2, id, token)
					if err != nil {
						report(name, false, "c2 登录 err: "+err.Error())
					} else if ec != errNone {
						report(name, false, fmt.Sprintf("c2 应替换成功但 ec=%d", ec))
					} else {
						report(name, true, "c2 替换成功 ec=0 (已超时)")
					}
				}
			}
		}
	}

	fmt.Printf("\n=== 结果: pass=%d fail=%d ===\n", pass, fail)
	if fail > 0 {
		os.Exit(1)
	}
}
