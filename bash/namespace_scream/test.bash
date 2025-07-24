#!/bin/bash
SCREAM_BIN="$HOME/research/l4s/scream/bin"
PORT_L4S=8081
TEST_TIME=30

# 1) Receiver in ns_r
sudo ip netns exec ns_r \
  $SCREAM_BIN/scream_bw_test_rx 172.20.1.2 $PORT_L4S \
  > rx_l4s.log 2>&1 &

sleep 1

# 2) Sender in ns_s  ← make sure this is namespaced!
sudo ip netns exec ns_s \
  $SCREAM_BIN/scream_bw_test_tx -ect 1 172.20.1.2 $PORT_L4S \
  > tx_l4s.log 2>&1 &

sleep $TEST_TIME

# 3) Cleanup
sudo pkill -f scream_bw_test_tx
sudo pkill -f scream_bw_test_rx
