#!/bin/bash

# === Configuration ===
TRACE_UP="../../traces/Verizon-LTE-short.up"
TRACE_DOWN="../../traces/Verizon-LTE-short.down"
DEFAULT_TIME=30   # Increased from 20 to 30 seconds
DELAY=10
TEST_TIME=${1:-$DEFAULT_TIME}

# Set this to the path where the scream binaries live (or leave empty to use PATH)
SCREAM_BIN_DIR="$HOME/research/l4s/scream/bin"

# === Check if binaries exist ===
TX_BIN="$SCREAM_BIN_DIR/scream_bw_test_tx"
if [[ ! -x "$TX_BIN" ]]; then
  echo "❌ SCReAM sender binary not found or not executable in $SCREAM_BIN_DIR"
  echo "Set SCREAM_BIN_DIR to the correct location or ensure scream_bw_test_tx is in your PATH"
  exit 1
fi

# === Define ports
PORT_L4S=8081
PORT_CLASSIC=8082

# === Print startup instructions for receiver
echo "[*] Please make sure the SCReAM receivers are running OUTSIDE Mahimahi:"
echo "Make sure to also give it at least 15-20 seconds for the receivers to reset even when you rerun them!"
echo "  L4S:     $SCREAM_BIN_DIR/scream_bw_test_rx 10.0.0.2 $PORT_L4S"
echo "  Classic: $SCREAM_BIN_DIR/scream_bw_test_rx 10.0.0.2 $PORT_CLASSIC"
echo
read -p "Press Enter to continue once receivers are started..."

# === Run test and graph
run_test () {
  local QUEUE=$1
  echo ""
  echo "[*] Running $QUEUE queue with SCReAM senders for $TEST_TIME seconds"

  mm-delay $DELAY mm-link --meter-all \
    --uplink-queue=$QUEUE \
    --uplink-queue-args="packets=100,interval=100,target=5" --uplink-log="output_mahimahi" \
    "$TRACE_UP" "$TRACE_DOWN" -- bash -c "
        echo '[+] Starting SCReAM Classic sender...'
        $TX_BIN 10.0.0.1 $PORT_CLASSIC > scream_tx_classic.log 2>&1 &

        echo '[⏳] Waiting 5 seconds before starting Classic sender (late-comer effect test)...'
        sleep 5

        echo '[+] Starting SCReAM L4S sender...'
        $TX_BIN -ect 1 10.0.0.1 $PORT_L4S > scream_tx_l4s.log 2>&1 &

        echo '[*] Running ps to check SCReAM processes:'
        ps aux | grep scream | grep -v grep

        sleep $TEST_TIME

        echo '[*] Killing SCReAM sender processes...'
        pkill -f scream_bw_test_tx
    "
}

# === Run Tests ===
echo "[*] Starting dualPI2 SCReAM test in 5 seconds..."
sleep 5
run_test dualPI2
echo "[*] FINISHED dualPI2 test."