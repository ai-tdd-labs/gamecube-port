#!/usr/bin/env bash
set -euo pipefail

source "$(cd "$(dirname "$0")/.." && pwd)/tools/helpers/lock.sh"
acquire_lock "gc-trace-replay" 600
trap release_lock EXIT

if [[ "${GC_ALLOW_DIRTY:-0}" != "1" ]] && ( ! git diff --quiet || ! git diff --cached --quiet ); then
  echo "fatal: git worktree has uncommitted changes; commit/stash first" >&2
  git status --porcelain=v1 >&2 || true
  exit 2
fi

case_dir=${1:?case_dir required}
repo_root="$(cd "$(dirname "$0")/.." && pwd)"
case_dir_abs="$case_dir"
if [[ "$case_dir_abs" != /* ]]; then
  case_dir_abs="$repo_root/$case_dir_abs"
fi

if [[ ! -d "$case_dir_abs" ]]; then
  echo "fatal: case dir not found: $case_dir_abs" >&2
  exit 2
fi

for f in in_regs.json out_regs.json in_module_info.bin out_module_info.bin in_os_module_list.bin out_os_module_list.bin; do
  if [[ ! -f "$case_dir_abs/$f" ]]; then
    echo "fatal: missing $case_dir_abs/$f" >&2
    exit 2
  fi
done

case_id="$(basename "$case_dir_abs")"
scenario="$repo_root/tests/sdk/os/os_unlink/host/os_unlink_rvz_trace_replay_001_scenario.c"

read_json_hex() {
  python3 - <<PY
import json
from pathlib import Path
obj=json.loads(Path("$1").read_text())
path="$2".split(".")
cur=obj
for k in path:
    cur=cur.get(k,{}) if isinstance(cur,dict) else {}
if isinstance(cur,str) and cur.startswith("0x"):
    print(cur)
else:
    print("")
PY
}

read_u32be_at() {
  python3 - <<PY
from pathlib import Path
p=Path("$1").read_bytes()
off=int("$2",0)
if off+4 > len(p):
    print(0)
else:
    print(int.from_bytes(p[off:off+4], "big"))
PY
}

in_old_ptr_hex="$(read_json_hex "$case_dir_abs/in_regs.json" "args.r3")"
expect_ret_hex="$(read_json_hex "$case_dir_abs/out_regs.json" "args.r3")"
if [[ -z "$in_old_ptr_hex" ]]; then
  echo "fatal: missing in_regs.args.r3 in $case_dir_abs/in_regs.json" >&2
  exit 2
fi
if [[ -z "$expect_ret_hex" ]]; then
  echo "fatal: missing out_regs.args.r3 in $case_dir_abs/out_regs.json" >&2
  exit 2
fi
in_old_ptr=$((in_old_ptr_hex))
expect_ret=$((expect_ret_hex))

in_prev="$(read_u32be_at "$case_dir_abs/in_module_info.bin" 0x08)"
in_next="$(read_u32be_at "$case_dir_abs/in_module_info.bin" 0x04)"
out_prev="$(read_u32be_at "$case_dir_abs/out_module_info.bin" 0x08)"
out_next="$(read_u32be_at "$case_dir_abs/out_module_info.bin" 0x04)"

in_head="$(read_u32be_at "$case_dir_abs/in_os_module_list.bin" 0x00)"
in_tail="$(read_u32be_at "$case_dir_abs/in_os_module_list.bin" 0x04)"
out_head="$(read_u32be_at "$case_dir_abs/out_os_module_list.bin" 0x00)"
out_tail="$(read_u32be_at "$case_dir_abs/out_os_module_list.bin" 0x04)"

GC_TRACE_CASE_ID="$case_id" \
GC_EXPECT_RET="$expect_ret" \
GC_IN_PREV_NONZERO="$([[ "$in_prev" != "0" ]] && echo 1 || echo 0)" \
GC_IN_NEXT_NONZERO="$([[ "$in_next" != "0" ]] && echo 1 || echo 0)" \
GC_IN_HEAD_NONZERO="$([[ "$in_head" != "0" ]] && echo 1 || echo 0)" \
GC_IN_TAIL_NONZERO="$([[ "$in_tail" != "0" ]] && echo 1 || echo 0)" \
GC_IN_HEAD_IS_OLD="$([[ "$in_head" == "$in_old_ptr" ]] && echo 1 || echo 0)" \
GC_IN_TAIL_IS_OLD="$([[ "$in_tail" == "$in_old_ptr" ]] && echo 1 || echo 0)" \
GC_OUT_PREV_NONZERO="$([[ "$out_prev" != "0" ]] && echo 1 || echo 0)" \
GC_OUT_NEXT_NONZERO="$([[ "$out_next" != "0" ]] && echo 1 || echo 0)" \
GC_OUT_HEAD_NONZERO="$([[ "$out_head" != "0" ]] && echo 1 || echo 0)" \
GC_OUT_TAIL_NONZERO="$([[ "$out_tail" != "0" ]] && echo 1 || echo 0)" \
GC_OUT_HEAD_IS_OLD="$([[ "$out_head" == "$in_old_ptr" ]] && echo 1 || echo 0)" \
GC_OUT_TAIL_IS_OLD="$([[ "$out_tail" == "$in_old_ptr" ]] && echo 1 || echo 0)" \
  bash "$repo_root/tools/run_host_scenario.sh" "$scenario"

out_bin="$repo_root/tests/sdk/os/os_unlink/actual/os_unlink_rvz_trace_$case_id.bin"
python3 - <<PY
from pathlib import Path
p = Path("$out_bin")
b = p.read_bytes()
def u32be(off):
    return int.from_bytes(b[off:off+4], "big")
marker = u32be(0x00)
exp_ret = u32be(0x04)
act_ret = u32be(0x08)
exp_prev = u32be(0x0C)
act_prev = u32be(0x10)
exp_next = u32be(0x14)
act_next = u32be(0x18)
exp_head = u32be(0x1C)
act_head = u32be(0x20)
exp_tail = u32be(0x24)
act_tail = u32be(0x28)
exp_head_old = u32be(0x2C)
act_head_old = u32be(0x30)
exp_tail_old = u32be(0x34)
act_tail_old = u32be(0x38)
print(f"case={p.name} marker=0x{marker:08X} exp_ret={exp_ret} act_ret={act_ret} exp_prev={exp_prev} act_prev={act_prev} exp_next={exp_next} act_next={act_next} exp_head={exp_head} act_head={act_head} exp_tail={exp_tail} act_tail={act_tail} exp_head_old={exp_head_old} act_head_old={act_head_old} exp_tail_old={exp_tail_old} act_tail_old={act_tail_old}")
if marker != 0xDEADBEEF:
    raise SystemExit(1)
PY

