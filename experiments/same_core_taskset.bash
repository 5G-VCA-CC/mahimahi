#!/usr/bin/env bash
set -euo pipefail

# Usage:
#   ./run_mahi_single_core.bash <seconds_per_run> <num_runs> [core]
#
# Examples:
#   ./run_mahi_single_core.bash 20 10      # 10 runs, 20s each, core=7
#   ./run_mahi_single_core.bash 30 5 3     # 5 runs, 30s each, core=3

SECS=${1:-20}
NUM_RUNS=${2:-1}
CORE=${3:-7}

OUT_DIR="./outputs"
mkdir -p "$OUT_DIR"

TRACE_UP="../traces/Constant.up"
TRACE_DOWN="../traces/Constant.down"

DELAY=0
PKTS=200
CLASSIC_TOS=0

PORT=5300
PACKET_LEN=1200
RATE="12M"

cleanup_network() {
  echo "[*] Cleaning up Mahimahi environment..."

  sudo pkill -9 -f "mm-link" 2>/dev/null || true
  sudo pkill -9 -f "mm-delay" 2>/dev/null || true
  sudo pkill -9 -f "mahimahi" 2>/dev/null || true
  sudo pkill -9 -f "iperf3" 2>/dev/null || true

  local ns_list
  ns_list=$(sudo ip netns list | awk '{print $1}' | grep -E '^mm-' || true)
  if [[ -n "${ns_list}" ]]; then
    while read -r ns; do
      [[ -z "$ns" ]] && continue
      echo "    [DEL] netns: $ns"
      sudo ip netns delete "$ns" 2>/dev/null || true
    done <<< "$ns_list"
  else
    echo "    [SKIP] No mm-* namespaces."
  fi

  echo "    [OK] Cleanup done."
}

# --- Sanity checks ---
if [[ ! -f "$TRACE_UP" || ! -f "$TRACE_DOWN" ]]; then
  echo "[!] Trace files not found:"
  echo "    UP:   $TRACE_UP"
  echo "    DOWN: $TRACE_DOWN"
  exit 1
fi

echo "[*] Runs: ${NUM_RUNS}"
echo "[*] Seconds per run: ${SECS}"
echo "[*] Pinning EVERYTHING to CPU core: ${CORE}"
echo "[*] Outputs: ${OUT_DIR}"
echo ""

for run in $(seq 1 "${NUM_RUNS}"); do
  echo "[*] ==============================="
  echo "[*] RUN ${run}/${NUM_RUNS}"
  echo "[*] ==============================="

  cleanup_network
  sleep 1

  OUT_FILE="${OUT_DIR}/output_run${run}.txt"
  FLOW_FILE="${OUT_DIR}/iperf_flow_run${run}.txt"

  echo "[*] Starting iperf3 server (core ${CORE})..."
  taskset -c "${CORE}" iperf3 -s -p "${PORT}" > /dev/null 2>&1 &
  SERVER_PID=$!
  sleep 1

  echo "[*] Launching Mahimahi + iperf client..."

  taskset -c "${CORE}" mm-delay "${DELAY}" mm-link --meter-all \
    --uplink-queue=dualPI2 \
    --uplink-queue-args="packets=${PKTS},target=16,tupdate=16,alpha=0.16,beta=3" \
    "${TRACE_UP}" "${TRACE_DOWN}" -- \
    taskset -c "${CORE}" bash -lc "
      echo '[+] run=${run} starting UDP flow';
      exec taskset -c ${CORE} iperf3 -c 10.0.0.1 -p ${PORT} -u -b ${RATE} \
        -l ${PACKET_LEN} -t ${SECS} --tos ${CLASSIC_TOS} --interval 1 \
        2>&1 | tee '${FLOW_FILE}'
    " >> "${OUT_FILE}" 2>&1

  echo "[*] Stopping server..."
  kill "${SERVER_PID}" 2>/dev/null || true

  echo "[*] Run ${run} complete."
  echo "[*] Cooling down 2s..."
  sleep 2
done

echo ""
echo "[*] All runs complete. Logs in: ${OUT_DIR}"
