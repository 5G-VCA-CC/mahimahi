#!/bin/bash
# Usage: ./run_many_combined.bash [num_runs] [seconds_per_run]
# Example: ./run_many_combined.bash 3 20

# ========== User Arguments ==========
NUM_RUNS=${1:-1}
SECS=${2:-20}
OUT_DIR="./outputs"
mkdir -p "$OUT_DIR"

# ========== Constants ==========
TRACE_UP="../traces/Constant.up"
TRACE_DOWN="../traces/Constant.down"
L4S_TOS=1
CLASSIC_TOS=0
DELAY=0
PKTS=200

# ========== Helper: Clean Up Mahimahi Environment ==========
cleanup_network() {
  echo "[*] Cleaning up Mahimahi environment..."

  kill_and_report() {
    local proc=$1
    if pgrep -f "$proc" >/dev/null; then
      echo "    [KILL] Found and killing: $proc"
      sudo pkill -9 "$proc" 2>/dev/null || true
    else
      echo "    [SKIP] No process found for: $proc"
    fi
  }

  kill_and_report "mahimahi"
  kill_and_report "mm-link"
  kill_and_report "mm-delay"
  kill_and_report "mm-meter"
  kill_and_report "iperf3"

  # Remove stale namespaces
  ns_list=$(sudo ip netns list | awk '{print $1}')
  if [[ -n "$ns_list" ]]; then
    for ns in $ns_list; do
      echo "    [DEL] Namespace: $ns"
      sudo ip netns delete "$ns" 2>/dev/null || true
    done
  else
    echo "    [SKIP] No leftover namespaces."
  fi

  # Delete any leftover veth interfaces
  veth_list=$(ip link show | grep -oE 'veth[^:@ ]+')
  if [[ -n "$veth_list" ]]; then
    for v in $veth_list; do
      echo "    [DEL] veth interface: $v"
      sudo ip link delete "$v" 2>/dev/null || true
    done
  else
    echo "    [SKIP] No leftover veth interfaces."
  fi

  # Flush qdiscs
  dev_list=$(ip -o link show | awk -F': ' '{print $2}')
  for dev in $dev_list; do
    if sudo tc qdisc show dev "$dev" 2>/dev/null | grep -q "qdisc"; then
      echo "    [FLUSH] qdisc on $dev"
      sudo tc qdisc del dev "$dev" root 2>/dev/null || true
    fi
  done
  echo "    [OK] Cleanup complete."
}

# ========== Run Loop ==========
RUNS_LEFT=$NUM_RUNS
NEXT_IDX=0

while [[ $RUNS_LEFT -gt 0 ]]; do
  OUT_FILE="$OUT_DIR/output_${NEXT_IDX}.txt"
  FLOW_FILE="/tmp/iperf_flow_${NEXT_IDX}.txt"

  if [[ -e "$OUT_FILE" ]]; then
    ((NEXT_IDX++))
    continue
  fi

  echo ""
  echo "[*] === RUN $((NUM_RUNS - RUNS_LEFT + 1))/$NUM_RUNS ==="

  # Check for hanging processes before starting
  if pgrep -f mm-link >/dev/null || pgrep -f iperf3 >/dev/null; then
    echo "[!] Detected leftover Mahimahi or iperf3 processes. Running cleanup..."
    cleanup_network
    sleep 2
  else
    echo "[*] Environment clean — no manual cleanup needed."
  fi

  echo "[*] Output log: $OUT_FILE"
  echo "[*] Flow log  : $FLOW_FILE"

  echo "[*] Starting iperf3 server on port 5300..."
  iperf3 -s -p 5300 > /dev/null 2>&1 &
  IPERF_SERVER_PID=$!

  echo "[*] Starting DualPI2 L4S test in 2 seconds..."
  sleep 2

  mm-delay $DELAY mm-link --meter-all \
    --uplink-queue=dualPI2 \
    --uplink-queue-args="\
      packets=${PKTS},\
      target=16,\
      tupdate=16,\
      alpha=0.16,\
      beta=3" \
    "$TRACE_UP" "$TRACE_DOWN" -- bash -c "
      echo '[+] Starting constant UDP iperf3 flow (port 5300)...'
      iperf3 -c 10.0.0.1 -p 5300 -u -b 12M -l 1200 -t $SECS --tos $CLASSIC_TOS --interval 1 \
        2>&1 | tee \"$FLOW_FILE\"
      echo '[+] Flow finished. Exiting Mahimahi shell...'
    " >> "$OUT_FILE" 2>&1

  echo "[*] FINISHED DualPI2 test. Cleaning up iperf3 server..."
  if kill $IPERF_SERVER_PID 2>/dev/null; then
    echo "    [KILL] iperf3 server terminated."
  else
    echo "    [SKIP] iperf3 server already stopped."
  fi

  echo "[*] Completed run $((NUM_RUNS - RUNS_LEFT + 1))/$NUM_RUNS"
  echo "[*] Cooling down 10s before next run..."
  sleep 10

  ((RUNS_LEFT--))
  ((NEXT_IDX++))
done

# Final cleanup if needed
if pgrep -f mm-link >/dev/null || pgrep -f iperf3 >/dev/null; then
  echo "[!] Performing final cleanup..."
  cleanup_network
else
  echo "[*] No leftover processes after final run. Clean exit."
fi

echo ""
echo "[*] All $NUM_RUNS run(s) complete. Logs saved in: $OUT_DIR/"