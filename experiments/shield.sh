#!/usr/bin/env bash
sudo cset set --destroy system
sudo cset set --destroy user
sudo cset set -l
set -euo pipefail

sudo cset shield --reset || true

# Put ALL desired experiment CPUs in the USER cpuset:
sudo cset shield --cpu=5-7 --kthread=off

# Run experiment (Mahimahi pinned to 7, iperf server 5, client 6)
sudo cset shield --exec -- \
  /home/linghe-zhang/l4s-research/mahimahi/experiments/same_core.bash 20 20 7 0 5 6



