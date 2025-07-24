# Start L4S receiver in ns_r
sudo ip netns exec ns_r /home/matthewzhang/research/l4s/scream/bin/scream_bw_test_rx 172.20.1.2 8081 > rx_l4s.log 2>&1 &

# Start Classic receiver in ns_r
# sudo ip netns exec ns_r /home/matthewzhang/research/l4s/scream/bin/scream_bw_test_rx 172.20.1.2 8082 > rx_classic.log 2>&1 &
