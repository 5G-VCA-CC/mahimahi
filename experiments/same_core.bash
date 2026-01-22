#!/usr/bin/env bash
set -euo pipefail

# Usage:
#   ./run_mahi_cset_single.bash <seconds_per_run> <num_runs> [mahi_core] [start_index] [server_core] [client_core]
#
# Examples:
#   ./run_mahi_cset_single.bash 20 10 7          # 10 runs, 20s each, mahi core 7, start 0, server 5, client 6 (defaults)
#   ./run_mahi_cset_single.bash 25 50 7 100      # 50 runs, start at output_cset100..149 (defaults server=5 client=6)
#   ./run_mahi_cset_single.bash 15 5             # 5 runs, 15s each, mahi core 0, server 5, client 6 (defaults)
#   ./run_mahi_cset_single.bash 20 10 7 0 5 6    # explicit: mahi=7, server=5, client=6

SECS=${1:-20}
NUM_RUNS=${2:-1}

# Core pinning
MAHI_CORE=${3:-0}     # Mahimahi + wrapper core
START_INDEX=${4:-0}
SERVER_CORE=${5:-5}   # iperf3 -s core
CLIENT_CORE=${6:-6}   # iperf3 -c core

OUT_DIR="./outputs"
TRACE_UP="../traces/Constant.up"
TRACE_DOWN="../traces/Constant.down"

DELAY=0
PKTS=200
CLASSIC_TOS=0

BASE_PORT=5300
PACKET_LEN=1200
RATE="12M"

# If invoked via sudo (e.g., sudo cset shield --exec -- ...),
# Mahimahi must still run as NON-ROOT. Prefer the original invoking user.
RUN_USER="${SUDO_USER:-$(id -un)}"
RUN_UID="$(id -u "$RUN_USER")"
RUN_GID="$(id -g "$RUN_USER")"

as_user() {
  sudo -u "$RUN_USER" --preserve-env=PATH,HOME,USER -- "$@"
}

ensure_outdir_writable() {
  sudo mkdir -p "$OUT_DIR"
  sudo chown -R "$RUN_UID:$RUN_GID" "$OUT_DIR" || true
  sudo chmod -R u+rwX "$OUT_DIR" || true
}

cleanup_network() {
  echo "[*] Cleaning up Mahimahi environment..."

  # Only kill your user's processes (prevents killing the cset wrapper)
  pkill -9 -u "$RUN_USER" -f "^mm-link"  2>/dev/null || true
  pkill -9 -u "$RUN_USER" -f "^mm-delay" 2>/dev/null || true
  pkill -9 -u "$RUN_USER" -f "iperf3 -s" 2>/dev/null || true
  pkill -9 -u "$RUN_USER" -f "iperf3 -c" 2>/dev/null || true

  # Delete only mm-* namespaces (requires sudo)
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

ensure_outdir_writable

echo "[*] Runs: ${NUM_RUNS}"
echo "[*] Seconds per run: ${SECS}"
echo "[*] Mahimahi core: ${MAHI_CORE}"
echo "[*] iperf3 server core: ${SERVER_CORE}"
echo "[*] iperf3 client core: ${CLIENT_CORE}"
echo "[*] Outputs: ${OUT_DIR} (output_cset<N>.txt)"
echo "[*] Mahimahi/iperf will run as user: ${RUN_USER}"

for r in $(seq 0 $((NUM_RUNS - 1))); do
  idx=$((START_INDEX + r))
  port=$((BASE_PORT + idx))  # unique port per run to avoid collisions if something lingers

  OUT_FILE="${OUT_DIR}/output_cset${idx}.txt"
  FLOW_FILE="${OUT_DIR}/iperf_flow_cset${idx}.txt"

  echo ""
  echo "[*] ==============================="
  echo "[*] RUN $((r + 1))/${NUM_RUNS}  (index=${idx}, port=${port})"
  echo "[*] ==============================="

  cleanup_network
  sleep 1

  echo "[*] Starting iperf3 server on port ${port} (pinned core ${SERVER_CORE})..."
  # Start server as RUN_USER and capture its PID
  server_pid="$(
    as_user bash -lc "taskset -c '${SERVER_CORE}' iperf3 -s -p '${port}' >/dev/null 2>&1 & echo \$!"
  )"
  sleep 1

  echo "[*] Launching Mahimahi (core ${MAHI_CORE}) -> ${OUT_FILE}"
  # Run mahimahi as RUN_USER (required), pinned to MAHI_CORE.
  # Append all stdout/stderr to OUT_FILE; iperf client output also tee'd to FLOW_FILE.
  as_user bash -lc "
    taskset -c '${MAHI_CORE}' mm-delay '${DELAY}' mm-link --meter-all \
      --uplink-queue=dualPI2 \
      --uplink-queue-args='packets=${PKTS},target=16,tupdate=16,alpha=0.16,beta=3' \
      '${TRACE_UP}' '${TRACE_DOWN}' -- bash -lc '
        echo \"[+] cset index=${idx} starting UDP flow -> port ${port}\";
        taskset -c ${CLIENT_CORE} iperf3 -c 10.0.0.1 -p ${port} -u -b ${RATE} -l ${PACKET_LEN} -t ${SECS} --tos ${CLASSIC_TOS} --interval 1 \
          2>&1 | tee \"${FLOW_FILE}\"
        echo \"[+] cset index=${idx} finished flow\";
      ' >> '${OUT_FILE}' 2>&1
  " || true

  echo "[*] Stopping server (pid=${server_pid})..."
  kill "$server_pid" 2>/dev/null || true

  echo "[*] Run index=${idx} complete."
  echo "[*] Cooling down 3s..."
  sleep 3
done

echo ""
echo "[*] All runs complete. Logs in: ${OUT_DIR}"
