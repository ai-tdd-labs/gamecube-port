#!/usr/bin/env bash
set -euo pipefail

# Safety: refuse to run if the worktree is dirty.
# Mutation checks intentionally dirty the worktree; allow bypass when GC_ALLOW_DIRTY=1.
if [[ "${GC_ALLOW_DIRTY:-0}" != "1" ]] && ( ! git diff --quiet || ! git diff --cached --quiet ); then
  echo "fatal: git worktree has uncommitted changes; commit/stash first" >&2
  git status --porcelain=v1 >&2 || true
  exit 2
fi


# Replay one retail RVZ PADRead trace case against sdk_port.
#
# Usage:
#   tools/replay_trace_case_pad_read.sh <trace_case_dir>

case_dir=${1:?trace case directory required}

repo_root="$(cd "$(dirname "$0")/.." && pwd)"

if [[ "$case_dir" != /* ]]; then
  case_dir="$repo_root/$case_dir"
fi

out_regs="$case_dir/out_regs.json"
out_status="$case_dir/out_status.bin"
case_id="$(basename "$case_dir")"

if [[ ! -f "$out_regs" || ! -f "$out_status" ]]; then
  echo "fatal: missing out_regs.json or out_status.bin in $case_dir" >&2
  exit 2
fi

vars="$(python3 - <<'PY' "$out_regs"
import json,sys
j=json.load(open(sys.argv[1],"r"))
ret=int(j["args"]["r3"],16)
print(f"RET={ret}")
PY
)"
eval "$vars"

scenario="$repo_root/tests/sdk/pad/pad_read/host/pad_read_rvz_trace_replay_001_scenario.c"

expected_dir="$repo_root/tests/sdk/pad/pad_read/expected"
actual_dir="$repo_root/tests/sdk/pad/pad_read/actual"
mkdir -p "$expected_dir" "$actual_dir"

expected_bin="$expected_dir/pad_read_rvz_trace_${case_id}.bin"
actual_bin="$actual_dir/pad_read_rvz_trace_${case_id}.bin"

python3 - <<'PY' "$expected_bin" "$RET" "$out_status"
import sys,struct
out=sys.argv[1]; ret=int(sys.argv[2]); status_path=sys.argv[3]
st=open(status_path,"rb").read()
if len(st)!=48:
  raise SystemExit(f"expected 48 bytes status, got {len(st)}")
def be32(x): return struct.pack(">I", x & 0xFFFFFFFF)
b = bytearray(b"\x00"*0x40)
b[0:4]=be32(0xDEADBEEF)
b[4:8]=be32(ret)
b[0x10:0x10+48]=st
open(out,"wb").write(b)
PY

GC_TRACE_CASE_ID="$case_id" \
  "$repo_root/tools/run_host_scenario.sh" "$scenario" >/dev/null

if [[ ! -f "$actual_bin" ]]; then
  echo "fatal: host run did not produce $actual_bin" >&2
  exit 2
fi

"$repo_root/tools/diff_bins.sh" "$expected_bin" "$actual_bin"
echo "[ok] $case_id (ret=$RET)"
