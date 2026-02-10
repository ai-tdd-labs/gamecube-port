#!/usr/bin/env bash
set -euo pipefail

# Safety: refuse to run if the worktree is dirty.
if ! git diff --quiet || ! git diff --cached --quiet; then
  echo "fatal: git worktree has uncommitted changes; commit/stash first" >&2
  git status --porcelain=v1 >&2 || true
  exit 2
fi


# Replay one retail RVZ VISetPostRetraceCallback trace case against sdk_port.
#
# Usage:
#   tools/replay_trace_case_vi_set_post_retrace_callback.sh <trace_case_dir>

case_dir=${1:?trace case directory required}

repo_root="$(cd "$(dirname "$0")/.." && pwd)"

if [[ "$case_dir" != /* ]]; then
  case_dir="$repo_root/$case_dir"
fi

in_regs="$case_dir/in_regs.json"
in_post="$case_dir/in_postcb.bin"
out_post="$case_dir/out_postcb.bin"
case_id="$(basename "$case_dir")"

if [[ ! -f "$in_regs" || ! -f "$in_post" || ! -f "$out_post" ]]; then
  echo "fatal: missing expected files in $case_dir" >&2
  exit 2
fi

vars="$(python3 - <<'PY' "$in_regs" "$in_post" "$out_post"
import json,struct,sys
in_regs_path, in_post_path, out_post_path = sys.argv[1], sys.argv[2], sys.argv[3]
in_regs=json.load(open(in_regs_path,"r"))
new_cb=int(in_regs["args"]["r3"],16)
old_cb=struct.unpack(">I", open(in_post_path,"rb").read(4))[0]
out_cb=struct.unpack(">I", open(out_post_path,"rb").read(4))[0]
if out_cb!=new_cb:
    print(f"fatal: out_postcb ({out_cb:#x}) != arg ({new_cb:#x})", file=sys.stderr)
    sys.exit(3)
seed_calls = 1 + (1 if old_cb else 0)
expect_disable = seed_calls
expect_restore = seed_calls
print(f"SEED_OLD={old_cb} SEED_NEW={new_cb} EXPECT_PTR={new_cb} EXPECT_SET_CALLS={seed_calls} EXPECT_DISABLE={expect_disable} EXPECT_RESTORE={expect_restore}")
PY
)"

eval "$vars"

scenario="$repo_root/tests/sdk/vi/vi_set_post_retrace_callback/host/vi_set_post_retrace_callback_rvz_trace_replay_001_scenario.c"

expected_dir="$repo_root/tests/sdk/vi/vi_set_post_retrace_callback/expected"
actual_dir="$repo_root/tests/sdk/vi/vi_set_post_retrace_callback/actual"
mkdir -p "$expected_dir" "$actual_dir"

expected_bin="$expected_dir/vi_set_post_retrace_callback_rvz_trace_${case_id}.bin"
actual_bin="$actual_dir/vi_set_post_retrace_callback_rvz_trace_${case_id}.bin"

python3 - <<'PY' "$expected_bin" "$SEED_OLD" "$SEED_NEW" "$EXPECT_PTR" "$EXPECT_SET_CALLS" "$EXPECT_DISABLE" "$EXPECT_RESTORE"
import sys,struct
out=sys.argv[1]
seed_old=int(sys.argv[2]); seed_new=int(sys.argv[3])
ptr=int(sys.argv[4]); set_calls=int(sys.argv[5])
disable_calls=int(sys.argv[6]); restore_calls=int(sys.argv[7])
def be32(x): return struct.pack(">I", x & 0xFFFFFFFF)
b=b"".join([
  be32(0xDEADBEEF),
  be32(seed_old),     # prev return
  be32(ptr),
  be32(set_calls),
  be32(disable_calls),
  be32(restore_calls),
  be32(seed_old),
  be32(seed_new),
])
b=b.ljust(0x40, b"\x00")
open(out,"wb").write(b)
PY

GC_TRACE_CASE_ID="$case_id" \
GC_SEED_OLD_CB="$SEED_OLD" \
GC_SEED_NEW_CB="$SEED_NEW" \
  "$repo_root/tools/run_host_scenario.sh" "$scenario" >/dev/null

if [[ ! -f "$actual_bin" ]]; then
  echo "fatal: host run did not produce $actual_bin" >&2
  exit 2
fi

"$repo_root/tools/diff_bins.sh" "$expected_bin" "$actual_bin"
echo "[ok] $case_id (old=$(printf '0x%X' "$SEED_OLD") new=$(printf '0x%X' "$SEED_NEW"))"

