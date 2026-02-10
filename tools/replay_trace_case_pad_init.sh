#!/usr/bin/env bash
set -euo pipefail

source "$(cd "$(dirname "$0")/.." && pwd)/tools/helpers/lock.sh"
acquire_lock "gc-trace-replay" 600
trap release_lock EXIT

# Safety: refuse to run if the worktree is dirty.
# Mutation checks intentionally dirty the worktree; allow bypass when GC_ALLOW_DIRTY=1.
if [[ "${GC_ALLOW_DIRTY:-0}" != "1" ]] && ( ! git diff --quiet || ! git diff --cached --quiet ); then
  echo "fatal: git worktree has uncommitted changes; commit/stash first" >&2
  git status --porcelain=v1 >&2 || true
  exit 2
fi

# Replay one retail RVZ PADInit trace case against sdk_port and assert that:
# - return value matches retail (out_regs r3)
# - sdk_state matches retail (out_sdk_state.bin)
#
# Usage:
#   tools/replay_trace_case_pad_init.sh <trace_case_dir>

case_dir=${1:?trace case directory required}

repo_root="$(cd "$(dirname "$0")/.." && pwd)"

if [[ "$case_dir" != /* ]]; then
  case_dir="$repo_root/$case_dir"
fi

case_id="$(basename "$case_dir")"

in_regs="$case_dir/in_regs.json"
out_regs="$case_dir/out_regs.json"
in_sdk="$case_dir/in_sdk_state.bin"
out_sdk="$case_dir/out_sdk_state.bin"

for f in "$in_regs" "$out_regs" "$in_sdk" "$out_sdk"; do
  if [[ ! -f "$f" ]]; then
    echo "fatal: missing $f" >&2
    exit 2
  fi
done

scenario="$repo_root/tests/sdk/pad/pad_init/host/pad_init_rvz_trace_replay_001_scenario.c"

expected_dir="$repo_root/tests/sdk/pad/pad_init/expected"
actual_dir="$repo_root/tests/sdk/pad/pad_init/actual"
mkdir -p "$expected_dir" "$actual_dir"

expected_bin="$expected_dir/pad_init_rvz_trace_${case_id}.bin"
actual_bin="$actual_dir/pad_init_rvz_trace_${case_id}.bin"

# Expected output = marker + ret + out_sdk_state (0x2000).
python3 - <<'PY' "$expected_bin" "$out_regs" "$out_sdk"
import json,sys
out_path=sys.argv[1]
out_regs_path=sys.argv[2]
out_sdk_path=sys.argv[3]

out_regs=json.load(open(out_regs_path,"r"))
args=out_regs.get("args",{}) or {}
ret=int(args.get("r3","0"),16) if "r3" in args else 0
ret=1 if ret!=0 else 0

sdk=open(out_sdk_path,"rb").read()
assert len(sdk)==0x2000

def be32(x:int)->bytes:
  return bytes([(x>>24)&0xFF,(x>>16)&0xFF,(x>>8)&0xFF,x&0xFF])

b = be32(0xDEADBEEF) + be32(ret) + sdk
assert len(b)==0x2008
open(out_path,"wb").write(b)
PY

GC_TRACE_CASE_ID="$case_id" \
GC_TRACE_IN_SDK_STATE="$in_sdk" \
GC_HOST_MAIN_DUMP_ADDR=0x80300000 \
GC_HOST_MAIN_DUMP_SIZE=0x2008 \
GC_HOST_DEBUG=0 \
tools/run_host_scenario.sh "$scenario"

"$repo_root/tools/diff_bins.sh" "$expected_bin" "$actual_bin"

echo "[ok] $case_id"

