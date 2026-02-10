#!/usr/bin/env bash
set -euo pipefail

# Replay one retail RVZ PADControlMotor trace case against sdk_port.
#
# Usage:
#   tools/replay_trace_case_pad_control_motor.sh <trace_case_dir>

case_dir=${1:?trace case directory required}

repo_root="$(cd "$(dirname "$0")/.." && pwd)"

if [[ "$case_dir" != /* ]]; then
  case_dir="$repo_root/$case_dir"
fi

in_regs="$case_dir/in_regs.json"
case_id="$(basename "$case_dir")"

if [[ ! -f "$in_regs" ]]; then
  echo "fatal: missing in_regs.json in $case_dir" >&2
  exit 2
fi

vars="$(python3 - <<'PY' "$in_regs"
import json,sys
j=json.load(open(sys.argv[1],"r"))
chan=int(j["args"]["r3"],16)
cmd=int(j["args"]["r4"],16)
print(f"CHAN={chan} CMD={cmd} EXPECTED={cmd}")
PY
)"

eval "$vars"

scenario="$repo_root/tests/sdk/pad/pad_control_motor/host/pad_control_motor_rvz_trace_replay_001_scenario.c"

expected_dir="$repo_root/tests/sdk/pad/pad_control_motor/expected"
actual_dir="$repo_root/tests/sdk/pad/pad_control_motor/actual"
mkdir -p "$expected_dir" "$actual_dir"

expected_bin="$expected_dir/pad_control_motor_rvz_trace_${case_id}.bin"
actual_bin="$actual_dir/pad_control_motor_rvz_trace_${case_id}.bin"

python3 - <<'PY' "$expected_bin" "$CHAN" "$CMD"
import sys,struct
out=sys.argv[1]; chan=int(sys.argv[2]); cmd=int(sys.argv[3])
def be32(x): return struct.pack(">I", x & 0xFFFFFFFF)
vals=[0,0,0,0]
if 0 <= chan < 4:
    vals[chan]=cmd
b=b"".join([
  be32(0xDEADBEEF),
  be32(chan),
  be32(cmd),
  be32(cmd),
  be32(vals[0]),
  be32(vals[1]),
  be32(vals[2]),
  be32(vals[3]),
])
b=b.ljust(0x40, b"\x00")
open(out,"wb").write(b)
PY

GC_TRACE_CASE_ID="$case_id" \
GC_SEED_CHAN="$CHAN" \
GC_SEED_CMD="$CMD" \
GC_EXPECTED_MOTOR_CMD="$EXPECTED" \
  "$repo_root/tools/run_host_scenario.sh" "$scenario" >/dev/null

if [[ ! -f "$actual_bin" ]]; then
  echo "fatal: host run did not produce $actual_bin" >&2
  exit 2
fi

"$repo_root/tools/diff_bins.sh" "$expected_bin" "$actual_bin"
echo "[ok] $case_id (chan=$CHAN cmd=$CMD)"

