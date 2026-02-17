#!/usr/bin/env bash
set -euo pipefail

BASE_CFG="${1:-exp.yaml}"
SWEEP_CFG="${2:-sweep.yaml}"
RUN_SCRIPT="${3:-./SHIELD_DUO.sh}"

STAMP="$(date +%Y%m%d_%H%M%S)"
OUTROOT="runs/sweep_${STAMP}"
mkdir -p "$OUTROOT"

# Extract sweep data once
SWEEP_JSON="$(python3 - "$SWEEP_CFG" <<'PY'
import yaml, json, sys
with open(sys.argv[1]) as f:
    s = yaml.safe_load(f)["sweep"]
print(json.dumps(s))
PY
)"

# Pull delays
readarray -t DELAYS < <(python3 - <<PY
import json
s=json.loads('''$SWEEP_JSON''')
for d in s["delays_ms"]:
    print(d)
PY
)

# Pull trace objects
readarray -t TRACE_NAMES < <(python3 - <<PY
import json
s=json.loads('''$SWEEP_JSON''')
for t in s["traces"]:
    print(t["name"])
PY
)

total_runs=$(( ${#DELAYS[@]} * ${#TRACE_NAMES[@]} ))
echo "[*] Total runs = ${#DELAYS[@]} × ${#TRACE_NAMES[@]} = $total_runs"
echo "[*] Output root: $OUTROOT"

run_idx=0

for delay in "${DELAYS[@]}"; do
  for tname in "${TRACE_NAMES[@]}"; do
    run_idx=$((run_idx + 1))

    rundir="${OUTROOT}/delay_${delay}/trace_${tname}"
    mkdir -p "$rundir"
    cfg="${rundir}/exp.yaml"

    # Patch exp.yaml: delay + traces.up/down
    python3 - "$BASE_CFG" "$cfg" "$delay" "$tname" "$SWEEP_JSON" <<'PY'
import sys, yaml, json
import os

src, dst = sys.argv[1], sys.argv[2]
delay = int(sys.argv[3])
tname = sys.argv[4]
sweep = json.loads(sys.argv[5])

# Find trace entry
trace = next(t for t in sweep["traces"] if t["name"] == tname)

with open(src) as f:
    d = yaml.safe_load(f) or {}

# --- PATCH ---
d.setdefault("mahimahi", {})["delay_ms"] = delay

d["traces"] = {
    "up": trace["up"],
    "down": trace["down"],
}

d["output_dir"] = os.path.dirname(os.path.abspath(dst))
# -------------

with open(dst, "w") as f:
    yaml.safe_dump(d, f, sort_keys=False)
PY

    echo "[*] ($run_idx/$total_runs) delay=${delay} trace=${tname}"
    ( set -x; bash "$RUN_SCRIPT" "$cfg" ) 2>&1 | tee "${rundir}/run.log"
  done
done
