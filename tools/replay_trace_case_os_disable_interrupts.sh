#!/usr/bin/env bash
set -euo pipefail

# Safety: refuse to run if the worktree is dirty.
if ! git diff --quiet || ! git diff --cached --quiet; then
  echo "fatal: git worktree has uncommitted changes; commit/stash first" >&2
  git status --porcelain=v1 >&2 || true
  exit 2
fi


# Replay one RVZ-traced OSDisableInterrupts case against sdk_port on host.
#
# Usage:
#   tools/replay_trace_case_os_disable_interrupts.sh <case_dir>
#
# Example:
#   tools/replay_trace_case_os_disable_interrupts.sh \
#     tests/traces/os_disable_interrupts/mp4_rvz/hit_000001_pc_800B723C_lr_800BAE98

case_dir=${1:?case_dir required}

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
case_dir_abs="$case_dir"
if [[ "$case_dir_abs" != /* ]]; then
  case_dir_abs="$repo_root/$case_dir_abs"
fi

case_id="$(basename "$case_dir_abs")"

scenario="$repo_root/tests/sdk/os/os_disable_interrupts/host/os_disable_interrupts_rvz_trace_replay_001_scenario.c"

msr_hex="$(python3 - <<PY
import json
from pathlib import Path
o = json.loads(Path("$case_dir_abs/in_regs.json").read_text())
print(o.get("msr") or "")
PY
)"

if [[ -z "$msr_hex" ]]; then
  echo "fatal: missing msr in $case_dir_abs/in_regs.json" >&2
  exit 2
fi

msr=$((msr_hex))
# PPC MSR[EE] (External Interrupt Enable) is bit 16 (mask 0x8000).
seed_enabled=0
if (( (msr & 0x8000) != 0 )); then
  seed_enabled=1
fi

GC_TRACE_CASE_ID="$case_id" \
GC_SEED_ENABLED="$seed_enabled" \
GC_EXPECTED_RET="$seed_enabled" \
  bash "$repo_root/tools/run_host_scenario.sh" "$scenario"

out_bin="$repo_root/tests/sdk/os/os_disable_interrupts/actual/os_disable_interrupts_rvz_trace_$case_id.bin"
python3 - <<PY
from pathlib import Path
p = Path("$out_bin")
b = p.read_bytes()
def u32be(off):
    return int.from_bytes(b[off:off+4], "big")
marker = u32be(0x00)
expected = u32be(0x04)
actual = u32be(0x08)
en_before = u32be(0x0C)
en_after = u32be(0x10)
disable_calls = u32be(0x14)
print(f"case={p.name} marker=0x{marker:08X} expected_ret={expected} actual_ret={actual} enabled_before=0x{en_before:08X} enabled_after=0x{en_after:08X} disable_calls={disable_calls}")
PY
