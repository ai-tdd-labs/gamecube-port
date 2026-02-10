#!/usr/bin/env bash
set -euo pipefail

# Replay one retail RVZ PADClamp trace case against sdk_port.
#
# Usage:
#   tools/replay_trace_case_pad_clamp.sh <trace_case_dir>

case_dir=${1:?trace case directory required}

repo_root="$(cd "$(dirname "$0")/.." && pwd)"

if [[ "$case_dir" != /* ]]; then
  case_dir="$repo_root/$case_dir"
fi

in_status="$case_dir/in_status.bin"
out_status="$case_dir/out_status.bin"
case_id="$(basename "$case_dir")"

if [[ ! -f "$in_status" || ! -f "$out_status" ]]; then
  echo "fatal: missing in_status.bin or out_status.bin in $case_dir" >&2
  exit 2
fi

scenario="$repo_root/tests/sdk/pad/pad_clamp/host/pad_clamp_rvz_trace_replay_001_scenario.c"

expected_dir="$repo_root/tests/sdk/pad/pad_clamp/expected"
actual_dir="$repo_root/tests/sdk/pad/pad_clamp/actual"
mkdir -p "$expected_dir" "$actual_dir"

expected_bin="$expected_dir/pad_clamp_rvz_trace_${case_id}.bin"
actual_bin="$actual_dir/pad_clamp_rvz_trace_${case_id}.bin"

python3 - <<'PY' "$expected_bin" "$out_status"
import sys,struct
out=sys.argv[1]; status_path=sys.argv[2]
st=open(status_path,"rb").read()
if len(st)!=48:
  raise SystemExit(f"expected 48 bytes status, got {len(st)}")
def be32(x): return struct.pack(">I", x & 0xFFFFFFFF)
b = bytearray(b"\x00"*0x40)
b[0:4]=be32(0xDEADBEEF)
b[0x10:0x10+48]=st
open(out,"wb").write(b)
PY

GC_TRACE_CASE_ID="$case_id" \
GC_TRACE_IN_STATUS="$in_status" \
  "$repo_root/tools/run_host_scenario.sh" "$scenario" >/dev/null

if [[ ! -f "$actual_bin" ]]; then
  echo "fatal: host run did not produce $actual_bin" >&2
  exit 2
fi

"$repo_root/tools/diff_bins.sh" "$expected_bin" "$actual_bin"
echo "[ok] $case_id"

