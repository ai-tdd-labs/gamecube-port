#!/usr/bin/env bash
set -euo pipefail

# Run a trace-guided constrained-random batch for PADControlMotor.
#
# Modes:
# - synthetic oracle (default): expected constructed from deterministic contract
# - dolphin oracle: expected dumped from DOL batch in Dolphin
#
# Usage:
#   tools/replay_trace_guided_pad_control_motor.sh [--seed 0xC0DE1234] [--count 128] [--oracle synthetic|dolphin] [--dolphin-suite trace_guided_batch_001|trace_guided_batch_002|exhaustive_matrix_001|max]

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
seed="0xC0DE1234"
count="128"
oracle="synthetic"
dolphin_suite="trace_guided_batch_001"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --seed) seed="$2"; shift 2 ;;
    --count) count="$2"; shift 2 ;;
    --oracle) oracle="$2"; shift 2 ;;
    --dolphin-suite) dolphin_suite="$2"; shift 2 ;;
    *) echo "unknown arg: $1" >&2; exit 2 ;;
  esac
done

expected_dir="$repo_root/tests/sdk/pad/pad_control_motor/expected"
actual_dir="$repo_root/tests/sdk/pad/pad_control_motor/actual"
scenario="$repo_root/tests/sdk/pad/pad_control_motor/host/pad_control_motor_rvz_trace_replay_001_scenario.c"

mkdir -p "$expected_dir" "$actual_dir"

if [[ "$oracle" == "synthetic" ]]; then
  cases_file="$repo_root/tests/trace-guided/pad_control_motor/cases/seed_${seed}_cases.jsonl"
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

  echo "PASS: PADControlMotor trace-guided synthetic batch ($total cases, seed=$seed)"
  exit 0
fi

if [[ "$oracle" == "dolphin" ]]; then
  run_one_dolphin_suite() {
    local suite="$1"
    local dol_dir=""
    local dol_elf=""
    local expected_bin=""
    local actual_bin=""
    local host_batch_scenario=""
    local dump_size=0
    local run_sec="0.7"

    case "$suite" in
      trace_guided_batch_001)
        [[ "$seed" == "0xC0DE1234" && "$count" == "64" ]] || { echo "fatal: $suite requires seed=0xC0DE1234 count=64" >&2; exit 2; }
        dol_dir="$repo_root/tests/sdk/pad/pad_control_motor/dol/generic/trace_guided_batch_001"
        dol_elf="$dol_dir/pad_control_motor_trace_guided_batch_001.dol"
        expected_bin="$expected_dir/pad_control_motor_trace_guided_batch_001.bin"
        actual_bin="$actual_dir/pad_control_motor_trace_guided_batch_001.bin"
        host_batch_scenario="$repo_root/tests/sdk/pad/pad_control_motor/host/pad_control_motor_trace_guided_batch_001_scenario.c"
        dump_size=$(( (4 + 64*8) * 4 ))
        ;;
      trace_guided_batch_002)
        [[ "$seed" == "0xC0DE1234" && "$count" == "2048" ]] || { echo "fatal: $suite requires seed=0xC0DE1234 count=2048" >&2; exit 2; }
        dol_dir="$repo_root/tests/sdk/pad/pad_control_motor/dol/generic/trace_guided_batch_002"
        dol_elf="$dol_dir/pad_control_motor_trace_guided_batch_002.dol"
        expected_bin="$expected_dir/pad_control_motor_trace_guided_batch_002.bin"
        actual_bin="$actual_dir/pad_control_motor_trace_guided_batch_002.bin"
        host_batch_scenario="$repo_root/tests/sdk/pad/pad_control_motor/host/pad_control_motor_trace_guided_batch_002_scenario.c"
        dump_size=$(( (4 + 2048*8) * 4 ))
        run_sec="1.0"
        ;;
      exhaustive_matrix_001)
        dol_dir="$repo_root/tests/sdk/pad/pad_control_motor/dol/generic/exhaustive_matrix_001"
        dol_elf="$dol_dir/pad_control_motor_exhaustive_matrix_001.dol"
        expected_bin="$expected_dir/pad_control_motor_exhaustive_matrix_001.bin"
        actual_bin="$actual_dir/pad_control_motor_exhaustive_matrix_001.bin"
        host_batch_scenario="$repo_root/tests/sdk/pad/pad_control_motor/host/pad_control_motor_exhaustive_matrix_001_scenario.c"
        dump_size=$(( (4 + 40*8) * 4 ))
        ;;
      *)
        echo "fatal: unsupported dolphin suite: $suite" >&2
        exit 2
        ;;
    esac

    make -C "$dol_dir" clean >/dev/null
    make -C "$dol_dir" >/dev/null
    "$repo_root/tools/dump_expected.sh" "$dol_elf" "$expected_bin" 0x80300000 "$dump_size" "$run_sec" >/dev/null
    "$repo_root/tools/run_host_scenario.sh" "$host_batch_scenario" >/dev/null
    "$repo_root/tools/diff_bins.sh" "$expected_bin" "$actual_bin" >/dev/null
    echo "PASS: PADControlMotor dolphin suite $suite"
  }

  if [[ "$dolphin_suite" == "max" ]]; then
    seed="0xC0DE1234"; count="64"; run_one_dolphin_suite trace_guided_batch_001
    run_one_dolphin_suite exhaustive_matrix_001
    seed="0xC0DE1234"; count="2048"; run_one_dolphin_suite trace_guided_batch_002
    echo "PASS: PADControlMotor dolphin suite max"
    exit 0
  fi

  run_one_dolphin_suite "$dolphin_suite"
  exit 0
fi

echo "fatal: unsupported --oracle value: $oracle" >&2
exit 2
