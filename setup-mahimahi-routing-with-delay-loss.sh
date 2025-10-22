#!/bin/bash

for pid in $(pgrep -f "mm-link" | tac); do
  # Skip the grep process itself
  if ps -p "$pid" -o args= | grep -q "grep"; then
    continue
  fi

  # Check for presence of 10.0.0.3 and 10.0.0.5 in the namespace
  if sudo nsenter -t "$pid" -n ip -br a 2>/dev/null | grep -q "10.0.0.3" && \
     sudo nsenter -t "$pid" -n ip -br a | grep -q "10.0.0.5"; then

    echo "Mahimahi router namespace PID is $pid"

    # Set up routing inside this namespace
    sudo nsenter -t "$pid" -n bash <<'EOF'
# Enable IP forwarding
sysctl -w net.ipv4.ip_forward=1

# Host → Mahimahi sender (10.0.0.2 → 10.0.0.5)
iptables -t nat -A PREROUTING -d 10.0.0.2 -j DNAT --to-destination 10.0.0.5
iptables -A FORWARD -d 10.0.0.5 -j ACCEPT

# Mahimahi sender → Host (10.0.0.3 → 10.0.0.1)
iptables -t nat -A PREROUTING -d 10.0.0.5 -j DNAT --to-destination 10.0.0.1
iptables -A FORWARD -d 10.0.0.1 -j ACCEPT
EOF

    break  # Stop after first match
  fi
done
