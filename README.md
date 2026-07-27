# Mahimahi DualPI2

---

The original Mahimahi README follows below.

Mahimahi is a set of lightweight network emulation tools for recording and replaying traffic under controlled link conditions (delay, loss, and rate). For an overview of the standard tools (`mm-delay`, `mm-link`, `mm-loss`, record/replay shells, and more), see the official documentation at [http://mahimahi.mit.edu/](http://mahimahi.mit.edu/).

## Using the DualPI2 module

---

To use the DualPI2 AQM, set it as the uplink and/or downlink queue type in `mm-link` via `--uplink-queue` and/or `--downlink-queue`, and pass DualPI2 parameters with `--uplink-queue-args` and/or `--downlink-queue-args`:

```bash
mm-link --uplink-queue=dualPI2 --uplink-queue-args="<ARGS>" \
  [--downlink-queue=dualPI2 --downlink-queue-args="<ARGS>"] \
  UPLINK-TRACE DOWNLINK-TRACE
```

You can configure uplink and downlink independently.

Concrete example with DualPI2 configured in the uplink direction only:

```bash
mm-link --uplink-queue=dualPI2 --uplink-queue-args="packets=100" \
  traces/fixed-12mbps.trace traces/fixed-12mbps.trace
```



### DualPI2 AQM arguments

Arguments are a comma-separated list of `NAME=VALUE` pairs. All of the following are **optional** (sensible defaults are applied when omitted):


| Parameter           | Description                                                         | Default         |
| ------------------- | ------------------------------------------------------------------- | --------------- |
| `packets`           | Queue size limit in packets. If set, takes precedence over `bytes`. | `10000`         |
| `bytes`             | Queue size limit in bytes. Used when `packets` is not set.          | `packets × MTU` |
| `target`            | PI² target queueing delay (ms).                                     | `15`            |
| `tupdate`           | Interval between probability updates (ms).                          | `16`            |
| `max_rtt`           | Maximum RTT assumed by the controller (ms).                         | `100`           |
| `alpha`             | PI integral gain.                                                   | `0.16`          |
| `beta`              | PI proportional gain.                                               | `3.2`           |
| `sched`             | Dual-queue scheduler type (`0` = WRR).                              | `0` (WRR)       |
| `l4s_max_threshold` | L4S (L-queue) marking threshold (ms).                               | `1`             |
| `l4s_min_threshold` | L4S ramp lower threshold (ms). If `0`, use a step function.         | `0` (step)      |
| `l4s_min_len`       | Minimum L-queue length (packets) before L4S marking.                | `1`             |




## Nesting `mm-delay`

---

Mahimahi shells compose by nesting. To add a fixed one-way delay around a DualPI2 link:

```bash
mm-delay <DELAY_MS> mm-link [OPTIONS] UPLINK-TRACE DOWNLINK-TRACE
```

Concrete example (10 ms one-way delay, DualPI2 uplink queue, live meters):

```bash
mm-delay 10 mm-link --meter-all --uplink-queue=dualPI2 --uplink-queue-args="packets=100" \
  traces/fixed-12mbps.trace traces/fixed-12mbps.trace
```



### Routing for delay + link nesting

When nesting `mm-delay` around `mm-link`, traffic between the inner and outer Mahimahi namespaces needs extra NAT/forwarding rules so the emulated endpoints can reach each other when the connection is initiated outside the Mahimahi environment. Run the delay+link routing helper **from inside the nested shell** (or from a script that runs inside it):

```bash
./setup-mahimahi-delay-routing.sh
```

This script finds the `mm-link` router namespace that bridges the delay and link interfaces and installs the DNAT/FORWARD rules needed for that two-shell setup.

## Nesting `mm-loss`

---

You can also nest `mm-loss` (typically between `mm-delay` and `mm-link`) to add stochastic packet loss:

```bash
mm-delay <DELAY_MS> mm-loss <DIRECTION> <LOSS_RATE> mm-link [OPTIONS] UPLINK-TRACE DOWNLINK-TRACE
```

Concrete example (10 ms one-way delay, 1% uplink loss, DualPI2 AQM, Verizon LTE traces):

```bash
mm-delay 10 mm-loss uplink 0.01 mm-link --uplink-queue=dualPI2 --uplink-queue-args="packets=100" \
  traces/Verizon-LTE-short.up traces/Verizon-LTE-short.down
```



### Routing for delay + loss + link nesting

With three nested shells (`mm-delay` → `mm-loss` → `mm-link`), use the three-shell routing helper instead:

```bash
./setup-mahimahi-delay-loss-routing.sh
```

This configures both the first-level and second-level Mahimahi router namespaces so packets are correctly forwarded across the delay, loss, and link layers.

---



# Mahimahi

A web performance measurement toolkit

## Install



### Requirements

On Ubuntu (at least), you need the following packages to install mahimahi:

- make
- autoconf
- libtool
- iproute2
- iptables
- dnsmasq
- apache2
- apache2-dev
- protobuf-compiler
- pkg-config
- libssl-dev
- libxcb-present-dev
- libpangomm-2.48-dev



### Install

Once all dependencies are met, you can install mahimahi by running:

- `./autogen.sh`
- `./configure`
- `make`
- `sudo make install`

