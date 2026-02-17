#!/usr/bin/env bash
# Run this after forcing prague system wide 
# or mahimahi won't use it in its created namespaces

set -euo pipefail

CFG="${1:-exp.yaml}"
RUN_USER="${SUDO_USER:-$(id -un)}"

if [[ $EUID -ne 0 ]]; then
  echo "[!] Must run as root (use: sudo $0 $CFG)"
  exit 1
fi

yaml_get() {
  local key="$1"
  if command -v yq >/dev/null 2>&1; then
    local v
    v="$(yq -r "$key // \"\"" "$CFG" 2>/dev/null || true)"
    [[ "$v" == "null" ]] && v=""
    echo "$v"
  else
    python3 - "$CFG" "$key" <<'PY'
import sys, yaml
cfg, key = sys.argv[1], sys.argv[2]
with open(cfg) as f:
    d = yaml.safe_load(f) or {}
cur = d
ok = True
for p in key.lstrip(".").split("."):
    if isinstance(cur, dict) and p in cur:
        cur = cur[p]
    else:
        ok = False
        break
if not ok or cur is None:
    print("")
else:
    print(cur)
PY
  fi
}

OUT_DIR="$(realpath -m "$(yaml_get '.output_dir')")"
SECS="$(yaml_get '.secs_per_run')"
NUM_RUNS="$(yaml_get '.num_runs')"

TRACE_UP="$(yaml_get '.traces.up')"
TRACE_DOWN="$(yaml_get '.traces.down')"

QUEUE_TYPE="$(yaml_get '.queue.type')"
Q_PACKETS="$(yaml_get '.queue.packets')"
Q_TARGET="$(yaml_get '.queue.target')"
Q_TUPDATE="$(yaml_get '.queue.tupdate')"
Q_ALPHA="$(yaml_get '.queue.alpha')"
Q_BETA="$(yaml_get '.queue.beta')"

BASE_PORT="$(yaml_get '.flows.base_port')"

CLASSIC_PROTO="$(yaml_get '.flows.classic.proto')"
CLASSIC_RATE="$(yaml_get '.flows.classic.rate')"
CLASSIC_PACKET_LEN="$(yaml_get '.flows.classic.packet_len')"
CLASSIC_TOS="$(yaml_get '.flows.classic.tos')"
CLASSIC_CC="$(yaml_get '.flows.classic.cc')"

L4S_TOS="$(yaml_get '.flows.l4s.tos')"
L4S_CC="$(yaml_get '.flows.l4s.cc')"

: "${SECS:=30}"
: "${NUM_RUNS:=1}"
: "${BASE_PORT:=5300}"

QUEUE_ARGS="packets=${Q_PACKETS},target=${Q_TARGET},tupdate=${Q_TUPDATE},alpha=${Q_ALPHA},beta=${Q_BETA}"
DELAY_MS="$(yaml_get '.mahimahi.delay_ms')"
: "${DELAY_MS:=0}"

mkdir -p "$OUT_DIR"
chown -R "$RUN_USER:$RUN_USER" "$OUT_DIR" || true

cleanup_all() {
  pkill -9 -x mm-link 2>/dev/null || true
  pkill -9 -x iperf3 2>/dev/null || true
}

cleanup_mm_netns() {
  command -v ip >/dev/null 2>&1 || return 0
  while read -r ns _; do
    [[ -z "${ns:-}" ]] && continue
    if [[ "$ns" =~ ^mm- ]] || [[ "$ns" =~ ^mahimahi ]] || [[ "$ns" =~ ^mml- ]]; then
      ip netns del "$ns" 2>/dev/null || true
    fi
  done < <(ip netns list 2>/dev/null || true)
}

next_index() {
  local max=-1
  shopt -s nullglob
  for f in "$OUT_DIR"/output_duo_*.txt; do
    local n="${f##*_}"
    n="${n%.txt}"
    [[ "$n" =~ ^[0-9]+$ ]] && (( n > max )) && max="$n"
  done
  shopt -u nullglob
  echo $((max + 1))
}

run_one() {
  local idx="$1"

  local classic_enabled=0
  local l4s_enabled=0

  if [[ -n "${CLASSIC_PROTO:-}" && "${CLASSIC_PROTO:-}" != "null" ]]; then
    classic_enabled=1
  fi
  if [[ -n "${L4S_TOS:-}" && "${L4S_TOS:-}" != "null" ]]; then
    l4s_enabled=1
    : "${L4S_CC:=prague}"
  fi

  if [[ $classic_enabled -eq 0 && $l4s_enabled -eq 0 ]]; then
    echo "[!] No flows enabled."
    exit 2
  fi

  local port1=$((BASE_PORT + 2*idx))
  local port2=$((BASE_PORT + 2*idx + 1))
  local out="$OUT_DIR/output_duo_${idx}.txt"

  cleanup_all
  cleanup_mm_netns
  sleep 1

  local srv1="" srv2=""
  if [[ $classic_enabled -eq 1 ]]; then
    iperf3 -s -p "$port1" >/dev/null 2>&1 &
    srv1=$!
  fi
  if [[ $l4s_enabled -eq 1 ]]; then
    iperf3 -s -p "$port2" >/dev/null 2>&1 &
    srv2=$!
  fi

  sleep 1

  sudo -u "$RUN_USER" \
      mm-delay "$DELAY_MS" \
      mm-link \
        --uplink-queue="$QUEUE_TYPE" \
        --uplink-queue-args="$QUEUE_ARGS" \
        "$TRACE_UP" "$TRACE_DOWN" -- \
        mm-delay "$DELAY_MS" \
        bash -lc "
          set -euo pipefail
          pids=()

          if [[ $classic_enabled -eq 1 ]]; then
            (
              iperf3 -c 10.0.0.1 -p $port1 \
                -C \"$CLASSIC_CC\" -t \"$SECS\" --tos \"$CLASSIC_TOS\"
            ) &
            pids+=(\$!)
          fi

          if [[ $l4s_enabled -eq 1 ]]; then
            (
              iperf3 -c 10.0.0.1 -p $port2 \
                -C \"$L4S_CC\" -t \"$SECS\" --tos \"$L4S_TOS\"
            ) &
            pids+=(\$!)
          fi

          for p in \"\${pids[@]}\"; do
            wait \"\$p\"
          done
        " 2>&1 | tee "$out"

  [[ -n "${srv1:-}" ]] && kill "$srv1" 2>/dev/null || true
  [[ -n "${srv2:-}" ]] && kill "$srv2" 2>/dev/null || true

  chown "$RUN_USER:$RUN_USER" "$out" || true
  cleanup_mm_netns
}

# ================================
# EXECUTION
# ================================

start="$(next_index)"
for ((i=0; i<NUM_RUNS; i++)); do
  run_one $((start + i))
  sleep 3
done

cleanup_all
cleanup_mm_netns
