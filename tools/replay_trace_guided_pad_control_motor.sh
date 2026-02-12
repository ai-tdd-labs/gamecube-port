#!/usr/bin/env bash
set -euo pipefail

# Run a trace-guided constrained-random batch for PADControlMotor.
# Uses host scenario replay with deterministic generated cases.
#
# Usage:
#   tools/replay_trace_guided_pad_control_motor.sh [--seed 0xC0DE1234] [--count 128]

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
seed="0xC0DE1234"
count="128"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --seed) seed="$2"; shift 2 ;;
    --count) count="$2"; shift 2 ;;
    *) echo "unknown arg: $1" >&2; exit 2 ;;
  esac
done

cases_file="$repo_root/tests/trace-guided/pad_control_motor/cases/seed_${seed}_cases.jsonl"
expected_dir="$repo_root/tests/sdk/pad/pad_control_motor/expected"
actual_dir="$repo_root/tests/sdk/pad/pad_control_motor/actual"
scenario="$repo_root/tests/sdk/pad/pad_control_motor/host/pad_control_motor_rvz_trace_replay_001_scenario.c"

mkdir -p "$expected_dir" "$actual_dir"

python3 "$repo_root/tools/generate_trace_guided_pad_control_motor_cases.py" \
  --seed "$seed" --count "$count" --out "$cases_file"

total=0
while IFS= read -r line; do
  [[ -z "$line" ]] && continue
  total=$((total + 1))

  case_id="$(python3 - <<'PY' "$line"
import json,sys
j=json.loads(sys.argv[1]); print(j["case_id"])
PY
)"
  chan="$(python3 - <<'PY' "$line"
import json,sys
j=json.loads(sys.argv[1]); print(j["chan"])
PY
)"
  cmd="$(python3 - <<'PY' "$line"
import json,sys
j=json.loads(sys.argv[1]); print(j["cmd"])
PY
)"
  expected="$(python3 - <<'PY' "$line"
import json,sys
j=json.loads(sys.argv[1]); print(j["expected_motor_cmd"])
PY
)"

  expected_bin="$expected_dir/pad_control_motor_rvz_trace_${case_id}.bin"
  actual_bin="$actual_dir/pad_control_motor_rvz_trace_${case_id}.bin"

  python3 - <<'PY' "$expected_bin" "$chan" "$cmd"
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
  GC_SEED_CHAN="$chan" \
  GC_SEED_CMD="$cmd" \
  GC_EXPECTED_MOTOR_CMD="$expected" \
    "$repo_root/tools/run_host_scenario.sh" "$scenario" >/dev/null

  "$repo_root/tools/diff_bins.sh" "$expected_bin" "$actual_bin" >/dev/null
done < "$cases_file"

echo "PASS: PADControlMotor trace-guided batch ($total cases, seed=$seed)"
