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

# Replay one retail RVZ PADSetSpec trace case against sdk_port and assert that:
# - sdk_state matches retail (out_sdk_state.bin)
#
# Usage:
#   tools/replay_trace_case_pad_set_spec.sh <trace_case_dir>

case_dir=${1:?trace case directory required}

repo_root="$(cd "$(dirname "$0")/.." && pwd)"

if [[ "$case_dir" != /* ]]; then
  case_dir="$repo_root/$case_dir"
fi

case_id="$(basename "$case_dir")"

in_regs="$case_dir/in_regs.json"
in_shadow="$case_dir/in_pad_spec_shadow.bin"
in_spec="$case_dir/in_spec.bin"
in_make_status="$case_dir/in_make_status.bin"
out_shadow="$case_dir/out_pad_spec_shadow.bin"
out_spec="$case_dir/out_spec.bin"
out_make_status="$case_dir/out_make_status.bin"

for f in "$in_regs" "$in_shadow" "$in_spec" "$in_make_status" "$out_shadow" "$out_spec" "$out_make_status"; do
  if [[ ! -f "$f" ]]; then
    echo "fatal: missing $f" >&2
    exit 2
  fi
done

spec="$(python3 - "$in_regs" <<'PY'
import json,sys
regs=json.load(open(sys.argv[1],"r"))
args=regs.get("args",{}) or {}
print(args.get("r3","0x0"))
PY
)"

scenario="$repo_root/tests/sdk/pad/pad_set_spec/host/pad_set_spec_rvz_trace_replay_001_scenario.c"

expected_dir="$repo_root/tests/sdk/pad/pad_set_spec/expected"
actual_dir="$repo_root/tests/sdk/pad/pad_set_spec/actual"
mkdir -p "$expected_dir" "$actual_dir"

expected_bin="$expected_dir/pad_set_spec_rvz_trace_${case_id}.bin"
actual_bin="$actual_dir/pad_set_spec_rvz_trace_${case_id}.bin"

# Expected output = marker + spec + out_shadow + out_spec + out_make_status.
python3 - "$expected_bin" "$spec" "$out_shadow" "$out_spec" "$out_make_status" <<'PY'
import struct,sys
out_path=sys.argv[1]
spec=int(sys.argv[2],0)
out_shadow=sys.argv[3]
out_spec=sys.argv[4]
out_make_status=sys.argv[5]

shadow=open(out_shadow,"rb").read()
spec_mem=open(out_spec,"rb").read()
make_status=open(out_make_status,"rb").read()
assert len(shadow)==4
assert len(spec_mem)==4
assert len(make_status)==4

def be32(x:int)->bytes:
  return struct.pack(">I", x & 0xFFFFFFFF)

b = be32(0xDEADBEEF) + be32(spec) + shadow + spec_mem + make_status
assert len(b)==0x14
open(out_path,"wb").write(b)
PY

GC_TRACE_CASE_ID="$case_id" \
GC_TRACE_SPEC="$spec" \
GC_TRACE_IN_PADSPEC_SHADOW="$in_shadow" \
GC_TRACE_IN_SPEC="$in_spec" \
GC_TRACE_IN_MAKE_STATUS="$in_make_status" \
GC_HOST_MAIN_DUMP_ADDR=0x80300000 \
GC_HOST_MAIN_DUMP_SIZE=0x14 \
GC_HOST_DEBUG=0 \
tools/run_host_scenario.sh "$scenario"

"$repo_root/tools/diff_bins.sh" "$expected_bin" "$actual_bin"

echo "[ok] $case_id"
