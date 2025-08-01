#!/bin/bash

echo '[+] Starting iperf3 Classic (port 5300)...'
iperf3 -c 10.0.0.1 -u -p 5300 -b 1M -t 30 &
./../scream/bin/scream_bw_test_tx -ect 1 10.0.0.1 8080
