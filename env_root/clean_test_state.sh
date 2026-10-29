#!/usr/bin/env bash
# 清理压测测试态 key（快速版）：每主节点本地 SCAN → RESP 编码 DEL → redis-cli --pipe 批量。
# 单 key 单 DEL（避免 CROSSSLOT），--pipe 走单连接（避免每 key 起一个 redis-cli 进程）。

RC="/e/RealServer/env_root/RedisCluster/redis-cli.exe"
PORTS="7000 7001 7002"
DEL_RE='^(player:|account:|session:|rolename:|offline:|actor:index:|cluster:actor:|actor:state:|battle:|room:|trace:|cluster:migrate:restored:|friend:|actor:recovery:|__lat_test)'

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

for port in $PORTS; do
  keys="$TMP/keys_$port.txt"
  resp="$TMP/resp_$port.txt"
  : > "$keys"
  cursor=0
  while true; do
    out=$("$RC" -p "$port" scan "$cursor" count 1000 2>/dev/null | tr -d '\r')
    cursor=$(echo "$out" | head -1)
    echo "$out" | tail -n +2 | grep -E "$DEL_RE" >> "$keys"
    [ "$cursor" = "0" ] && break
  done
  n=$(wc -l < "$keys")
  if [ "$n" -gt 0 ]; then
    awk '{ printf "*2\r\n$3\r\nDEL\r\n$%d\r\n%s\r\n", length($0), $0 }' "$keys" > "$resp"
    "$RC" -p "$port" --pipe < "$resp" > /dev/null 2>&1
  fi
  echo "port=$port deleted=$n"
done
