#!/usr/bin/env bash
set -euo pipefail

# Get all mm-link PIDs once at the start
ALL_PIDS=$(pgrep -f "mm-link" || true)

if [ -z "$ALL_PIDS" ]; then
  echo "[-] No mm-link processes found"
  exit 1
fi

# Helper: find first mm-link pid that contains both IPs in a single batch operation
# Usage: find_router_pid ipA ipB
find_router_pid() {
  local ipA="$1"
  local ipB="$2"
  
  # Single sudo call that checks all PIDs and returns the first match
  local result
  result=$(sudo bash -c '
    ipA="$1"
    ipB="$2"
    shift 2
    
    for pid in "$@"; do
      [ -z "$pid" ] && continue
      
      # Try nsenter and capture output
      ns_output=$(nsenter -t "$pid" -n ip -br a 2>/dev/null || true)
      
      # If nsenter failed, skip
      if [ -z "$ns_output" ]; then
        continue
      fi
      
      # Test both IPs against the captured output
      if printf "%s" "$ns_output" | grep -q "$ipA" && printf "%s" "$ns_output" | grep -q "$ipB"; then
        printf "%s" "$pid"
        exit 0
      fi
    done
    exit 1
  ' _ "$ipA" "$ipB" $ALL_PIDS || true)
  
  if [ -n "$result" ]; then
    printf '%s' "$result"
    return 0
  fi
  
  return 1
}

# Main: configure two router namespaces (first-level and second-level)
# Find both PIDs in parallel for maximum speed
echo "[+] Searching for router namespaces..."

# Search for both routers in a single sudo session
ROUTER_PIDS=$(sudo bash -c '
  shift  # skip the placeholder
  
  first_level=""
  second_level=""
  
  for pid in "$@"; do
    [ -z "$pid" ] && continue
    
    # Get IP addresses once per PID
    ns_output=$(nsenter -t "$pid" -n ip -br a 2>/dev/null || true)
    [ -z "$ns_output" ] && continue
    
    # Check if this is first-level router (10.0.0.2 & 10.0.0.3)
    if [ -z "$first_level" ]; then
      if printf "%s" "$ns_output" | grep -q "10.0.0.2" && printf "%s" "$ns_output" | grep -q "10.0.0.3"; then
        first_level="$pid"
      fi
    fi
    
    # Check if this is second-level router (10.0.0.3 & 10.0.0.5)
    if [ -z "$second_level" ]; then
      if printf "%s" "$ns_output" | grep -q "10.0.0.3" && printf "%s" "$ns_output" | grep -q "10.0.0.5"; then
        second_level="$pid"
      fi
    fi
    
    # Exit early if both found
    if [ -n "$first_level" ] && [ -n "$second_level" ]; then
      break
    fi
  done
  
  printf "%s %s" "$first_level" "$second_level"
' _ $ALL_PIDS)

# Parse the results
read -r FIRST_PID SECOND_PID <<< "$ROUTER_PIDS"

# 1) Configure first-level router
if [ -n "$FIRST_PID" ]; then
  echo "[+] Mahimahi first-level router namespace PID is $FIRST_PID"

  sudo nsenter -t "$FIRST_PID" -n bash -c '
    set -x
    echo "[+] First-level: PREROUTING 10.0.0.2 → 10.0.0.4"
    iptables -t nat -A PREROUTING -d 10.0.0.2 -j DNAT --to-destination 10.0.0.4
    iptables -A FORWARD -d 10.0.0.4 -j ACCEPT

    echo "[+] First-level: PREROUTING 10.0.0.3 → 10.0.0.1"
    iptables -t nat -A PREROUTING -d 10.0.0.3 -j DNAT --to-destination 10.0.0.1
    iptables -A FORWARD -d 10.0.0.1 -j ACCEPT
  '
else
  echo "[-] First-level router (10.0.0.2 & 10.0.0.3) not found"
fi

# 2) Configure second-level router
if [ -n "$SECOND_PID" ]; then
  echo "[+] Mahimahi second-level router namespace PID is $SECOND_PID"

  sudo nsenter -t "$SECOND_PID" -n bash -c '
    set -x
    echo "[+] Second-level: PREROUTING 10.0.0.4 → 10.0.0.5"
    iptables -t nat -A PREROUTING -d 10.0.0.4 -j DNAT --to-destination 10.0.0.5
    iptables -A FORWARD -d 10.0.0.5 -j ACCEPT

    echo "[+] Second-level: PREROUTING 10.0.0.2 → 10.0.0.3"
    iptables -t nat -A PREROUTING -d 10.0.0.2 -j DNAT --to-destination 10.0.0.3
    iptables -A FORWARD -d 10.0.0.3 -j ACCEPT
  '
else
  echo "[-] Second-level router (10.0.0.3 & 10.0.0.5) not found"
fi

echo "[+] Done."
