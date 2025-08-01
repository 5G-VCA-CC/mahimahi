#!/bin/bash

echo '[+] Starting iperf3 L4S (port 5301)...'
iperf3 -c 10.0.0.1 -u -p 5301 -b 1M -t 30 --tos 0x01 &
echo '[+] Starting iperf3 Classic (port 5300)...'
iperf3 -c 10.0.0.1 -u -p 5300 -b 1M -t 30 &
