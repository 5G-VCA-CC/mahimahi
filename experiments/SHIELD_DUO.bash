#!/usr/bin/env bash
sudo cset set --destroy system
sudo cset set --destroy user
sudo cset set -l
#!/usr/bin/env bash
set -euo pipefail

SECS=${1:-20}
NUM_RUNS=${2:-1}

# Pick the user who invoked sudo, otherwise current user
RUN_USER="${SUDO_USER:-$(id -un)}"

HOUSEKEEPING="0-3"
EXPERIMENT_CPUS="4-7"

MAHI_CORE=7
SERVER_CORE=5
L4S_CLIENT_CORE=6
CLASSIC_CLIENT_CORE=4

echo "[*] Resetting cset..."
sudo cset shield --reset || true

echo "[*] Shielding CPUs: $EXPERIMENT_CPUS (housekeeping: $HOUSEKEEPING)"
sudo cset shield --cpu="$EXPERIMENT_CPUS" --kthread=on

for i in $(seq 0 $((NUM_RUNS - 1))); do
  echo ""
  echo "[*] ===== RUN $i ====="

  # Execute inside the shield, but drop privileges to RUN_USER so Mahimahi is non-root
  sudo cset shield --exec -- sudo -u "$RUN_USER" --preserve-env=PATH,HOME,USER bash -lc "
    export SERVER_CORE=$SERVER_CORE
    export L4S_CLIENT_CORE=$L4S_CLIENT_CORE
    export CLASSIC_CLIENT_CORE=$CLASSIC_CLIENT_CORE
    taskset -c $MAHI_CORE ./mahi_experiment_duo.bash $SECS $i
  "

  sleep 3
done

echo "[*] Releasing cset..."
sudo cset shield --reset
