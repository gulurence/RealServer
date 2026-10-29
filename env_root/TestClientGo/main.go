package main

// TestClient (Go) — RealServer 压测客户端。
// 用协程 (goroutine) 实现多虚拟客户端并发, 一个进程可同时跑数千客户端,
// 替代旧 C++ TestClient「每客户端一个进程」的低效模式。
//
// 两种模式:
//   --mode login  : 登录→创角→选角→驻留 --hold-ms (测在线上限)
//   --mode battle : 登录→创角→选角→建房→开局→直连 Battle→帧同步 (测战斗上限)

import (
	"flag"
	"fmt"
	"os"
	"sort"
	"sync"
	"sync/atomic"
	"time"
)

type args struct {
	gateHost      string
	gatePort      int
	count         int
	concurrency   int
	mode          string
	battleFrames  uint32
	frameInterval time.Duration
	poolBase      int64
	hold          time.Duration
	flowTimeout   time.Duration
}

func main() {
	a := args{}
	flag.StringVar(&a.gateHost, "gate-host", "127.0.0.1", "Gate 地址")
	flag.IntVar(&a.gatePort, "gate-port", 10080, "Gate 端口")
	flag.IntVar(&a.count, "count", 1, "虚拟客户端数量")
	flag.IntVar(&a.concurrency, "concurrency", 0, "并发上限 (0=不限, 等于 count)")
	flag.StringVar(&a.mode, "mode", "login", "模式: login|battle")
	var bf uint64 = 600
	flag.Uint64Var(&bf, "battle-frames", 600, "每场战斗输入帧数")
	frameMs := flag.Int("frame-interval-ms", 50, "帧间隔毫秒 (默认 50=20Hz)")
	flag.Int64Var(&a.poolBase, "pool-base", 1000000, "预建账号池起始 account_id")
	holdMs := flag.Int("hold-ms", 60000, "在线模式驻留毫秒")
	flowSec := flag.Int("flow-timeout", 60, "单客户端全链路超时秒")
	spawnMs := flag.Int("spawn-interval-ms", 0, "协程孵化间隔毫秒 (0=立即全量; 用于斜坡, 避免击穿 listen backlog)")
	flag.Parse()

	a.battleFrames = uint32(bf)
	a.frameInterval = time.Duration(*frameMs) * time.Millisecond
	a.hold = time.Duration(*holdMs) * time.Millisecond
	a.flowTimeout = time.Duration(*flowSec) * time.Second

	if a.mode != "login" && a.mode != "battle" {
		fmt.Fprintf(os.Stderr, "invalid --mode %q (want login|battle)\n", a.mode)
		os.Exit(2)
	}
	if a.concurrency <= 0 {
		a.concurrency = a.count
	}
	if a.concurrency > a.count {
		a.concurrency = a.count
	}

	fmt.Printf("[GoTestClient] mode=%s count=%d concurrency=%d gate=%s:%d pool-base=%d\n",
		a.mode, a.count, a.concurrency, a.gateHost, a.gatePort, a.poolBase)
	if a.mode == "battle" {
		fmt.Printf("[GoTestClient] battle frames=%d interval=%dms\n", a.battleFrames, *frameMs)
	}

	res := make(chan result, a.count)
	var done int64 // 已完成客户端计数 (进度显示用)

	sem := make(chan struct{}, a.concurrency)
	var wg sync.WaitGroup

	// 进度打印: 每秒输出一次完成数
	stopProgress := make(chan struct{})
	go func() {
		t := time.NewTicker(2 * time.Second)
		defer t.Stop()
		for {
			select {
			case <-t.C:
				fmt.Fprintf(os.Stderr, "  进度: %d/%d 完成\n", atomic.LoadInt64(&done), a.count)
			case <-stopProgress:
				return
			}
		}
	}()

	start := time.Now()
	spawnInterval := time.Duration(*spawnMs) * time.Millisecond
	for i := 1; i <= a.count; i++ {
		wg.Add(1)
		sem <- struct{}{}
		go func(id int) {
			defer wg.Done()
			defer func() { <-sem }()
			runClient(id, &a, res)
			atomic.AddInt64(&done, 1)
		}(i)
		if spawnInterval > 0 && i < a.count {
			time.Sleep(spawnInterval)
		}
	}

	// 收集协程结束
	go func() {
		wg.Wait()
		close(res)
		close(stopProgress)
	}()

	// 聚合
	var (
		ok, fail            int
		latencies           []int64
		errCodeHist         = map[int]int{}
		errMsgHist          = map[string]int{}
		totalSent, totalRcvd int64
	)
	for r := range res {
		if r.ok {
			ok++
			latencies = append(latencies, r.latencyMs)
			totalSent += int64(r.framesSent)
			totalRcvd += r.framesRcvd
		} else {
			fail++
			if r.errCode != 0 {
				errCodeHist[r.errCode]++
			} else {
				errMsgHist[r.errMsg]++
			}
		}
	}
	elapsed := time.Since(start)

	// 汇总
	fmt.Printf("\n[GoTestClient] ===== 结果 =====\n")
	fmt.Printf("模式=%s 总数=%d 成功=%d 失败=%d 耗时=%.2fs\n",
		a.mode, a.count, ok, fail, elapsed.Seconds())
	fmt.Printf("成功率=%.2f%% 吞吐=%.1f 客户端/s\n",
		float64(ok)/float64(a.count)*100, float64(a.count)/elapsed.Seconds())

	if len(latencies) > 0 {
		sort.Slice(latencies, func(i, j int) bool { return latencies[i] < latencies[j] })
		fmt.Printf("全链路耗时(ms) min=%d avg=%d p50=%d p95=%d p99=%d max=%d\n",
			latencies[0], avgI64(latencies), pct(latencies, 0.50), pct(latencies, 0.95),
			pct(latencies, 0.99), latencies[len(latencies)-1])
	}
	if a.mode == "battle" {
		fmt.Printf("帧同步: 总发送=%d 总接收=%d 平均接收/场=%.1f\n",
			totalSent, totalRcvd, float64(totalRcvd)/float64(maxInt(ok, 1)))
	}

	if fail > 0 {
		fmt.Printf("\n失败明细:\n")
		for code, n := range errCodeHist {
			fmt.Printf("  err_code=%d: %d 个\n", code, n)
		}
		for msg, n := range errMsgHist {
			fmt.Printf("  %s: %d 个\n", msg, n)
		}
	}
}

func maxInt(a, b int) int {
	if a > b {
		return a
	}
	return b
}

func avgI64(v []int64) int64 {
	var s int64
	for _, x := range v {
		s += x
	}
	return s / int64(len(v))
}

func pct(sorted []int64, p float64) int64 {
	if len(sorted) == 0 {
		return 0
	}
	i := int(float64(len(sorted)-1) * p)
	return sorted[i]
}
