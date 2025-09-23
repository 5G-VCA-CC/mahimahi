#!/bin/bash
# Goal: make the two nested (mm-link & mm-delay) p2p links communicate

# Search for the first mm-link process that can see both the 
# 10.0.0.1 ⟷  10.0.0.2 and the 10.0.0.3 ⟷  10.0.0.4 links
for pid in $(pgrep -f "mm-link"); do
  if sudo nsenter -t "$pid" -n ip -br a 2>/dev/null | grep -q "10.0.0.2" && \
     sudo nsenter -t "$pid" -n ip -br a | grep -q "10.0.0.3"; then
    echo "Mahimahi router namespace PID is $pid"
    # Run routing setup inside the router namespace
    sudo nsenter -t "$pid" -n bash -c "
      echo '[+] Enabling IP forwarding...'
      sysctl -w net.ipv4.ip_forward=1
      echo '[+] Routing 10.0.0.2 → 10.0.0.4'
      iptables -t nat -A PREROUTING -d 10.0.0.2 -j DNAT --to-destination 10.0.0.4
      iptables -A FORWARD -d 10.0.0.4 -j ACCEPT
      echo '[+] Routing 10.0.0.3 → 10.0.0.1'
      iptables -t nat -A PREROUTING -d 10.0.0.3 -j DNAT --to-destination 10.0.0.1
      iptables -A FORWARD -d 10.0.0.1 -j ACCEPT
    "
    break
  fi
done
