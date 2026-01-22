#!/usr/bin/env bash
set -euo pipefail

# File: mahi_experiment_l4s_single.bash
# Runs ONE UDP flow marked as "L4S" via --tos $L4S_TOS inside Mahimahi.
#
# Usage:
#   ./mahi_experiment_l4s_single.bash <seconds> <idx>

SECS=${1:-20}
IDX=${2:-0}

OUT_DIR="./outputs"
TRACE_UP="../traces/Constant.up"
TRACE_DOWN="../traces/Constant.down"

QUEUE="dualPI2"
QUEUE_ARGS="packets=200,target=16,tupdate=16,alpha=0.16,beta=3"

BASE_PORT=5300
PORT_L4S=$((BASE_PORT + IDX))

RATE="12M"
PACKET_LEN=1200

L4S_TOS="${L4S_TOS:-1}"          # set to your exact value if different
SERVER_CORE="${SERVER_CORE:-5}"
L4S_CLIENT_CORE="${L4S_CLIENT_CORE:-6}"

OUT_FILE="${OUT_DIR}/output_l4s_${IDX}.txt"
FLOW_L4S="${OUT_DIR}/iperf_l4s_${IDX}.txt"

mkdir -p "$OUT_DIR"

echo "[*] Cleaning up Mahimahi + iperf..."
sudo pkill -9 -f mm-link 2>/dev/null || true
sudo pkill -9 -f mm-delay 2>/dev/null || true
sudo pkill -9 -f iperf3 2>/dev/null || true

for ns in $(sudo ip netns list | awk '{print $1}' | grep '^mm-' || true); do
  sudo ip netns del "$ns" 2>/dev/null || true
done

sleep 1

echo "[*] Starting iperf3 server on port $PORT_L4S (core $SERVER_CORE)..."
taskset -c "$SERVER_CORE" iperf3 -s -p "$PORT_L4S" >/dev/null 2>&1 &
PID_SRV=$!

sleep 1

echo "[*] Running Mahimahi single L4S-marked UDP flow (IDX=$IDX)..."

# IMPORTANT: mm-delay/mm-link must run NON-ROOT
mm-delay 0 mm-link --meter-all \
  --uplink-queue="$QUEUE" \
  --uplink-queue-args="$QUEUE_ARGS" \
  "$TRACE_UP" "$TRACE_DOWN" -- bash -c "
    echo '[+] L4S flow: port=$PORT_L4S tos=$L4S_TOS'
    taskset -c $L4S_CLIENT_CORE iperf3 -c 10.0.0.1 -p $PORT_L4S -u \
      -b $RATE -l $PACKET_LEN -t $SECS --tos $L4S_TOS --interval 1 \
      2>&1 | tee $FLOW_L4S
  " | tee "$OUT_FILE"

echo "[*] Stopping server..."
kill $PID_SRV 2>/dev/null || true

echo "[*] Done. Logs in $OUT_DIR"
