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

case_id="$(basename "$case_dir_abs")"
scenario="$repo_root/tests/sdk/os/os_link/host/os_link_rvz_trace_replay_001_scenario.c"

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

expect_ret_hex="$(read_json_hex "$case_dir_abs/out_regs.json" "args.r3")"
if [[ -z "$expect_ret_hex" ]]; then
  echo "fatal: missing out_regs.args.r3 in $case_dir_abs/out_regs.json" >&2
  exit 2
fi
expect_ret=$((expect_ret_hex))

out_prev="$(read_u32be_at "$case_dir_abs/out_module_info.bin" 0x08)"
out_next="$(read_u32be_at "$case_dir_abs/out_module_info.bin" 0x04)"
out_head="$(read_u32be_at "$case_dir_abs/out_os_module_list.bin" 0x00)"
out_tail="$(read_u32be_at "$case_dir_abs/out_os_module_list.bin" 0x04)"
seed_has_tail=0
if [[ "$out_prev" != "0" ]]; then
  seed_has_tail=1
fi

GC_TRACE_CASE_ID="$case_id" \
GC_EXPECT_RET="$expect_ret" \
GC_EXPECT_PREV_NONZERO="$([[ "$out_prev" != "0" ]] && echo 1 || echo 0)" \
GC_EXPECT_NEXT_NONZERO="$([[ "$out_next" != "0" ]] && echo 1 || echo 0)" \
GC_EXPECT_HEAD_NONZERO="$([[ "$out_head" != "0" ]] && echo 1 || echo 0)" \
GC_EXPECT_TAIL_NONZERO="$([[ "$out_tail" != "0" ]] && echo 1 || echo 0)" \
GC_SEED_HAS_TAIL="$seed_has_tail" \
  bash "$repo_root/tools/run_host_scenario.sh" "$scenario"

out_bin="$repo_root/tests/sdk/os/os_link/actual/os_link_rvz_trace_$case_id.bin"
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
print(f"case={p.name} marker=0x{marker:08X} exp_ret={exp_ret} act_ret={act_ret} exp_prev={exp_prev} act_prev={act_prev} exp_next={exp_next} act_next={act_next} exp_head={exp_head} act_head={act_head} exp_tail={exp_tail} act_tail={act_tail}")
if marker != 0xDEADBEEF:
    raise SystemExit(1)
PY
