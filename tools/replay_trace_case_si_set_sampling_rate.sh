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


# Replay one retail RVZ SISetSamplingRate trace case against sdk_port and assert
# the observed outputs (derived from the trace) match our implementation.
#
# Usage:
#   tools/replay_trace_case_si_set_sampling_rate.sh <trace_case_dir>
#
# Example:
#   tools/replay_trace_case_si_set_sampling_rate.sh \
#     tests/trace-harvest/si_set_sampling_rate/mp4_rvz_si_ctrl/hit_000001_pc_800DA3C8_lr_800D961C

case_dir=${1:?trace case directory required}

repo_root="$(cd "$(dirname "$0")/.." && pwd)"

if [[ "$case_dir" != /* ]]; then
  case_dir="$repo_root/$case_dir"
fi

in_regs="$case_dir/in_regs.json"
out_si="$case_dir/out_si_ctrl.bin"
case_id="$(basename "$case_dir")"

if [[ ! -f "$in_regs" || ! -f "$out_si" ]]; then
  echo "fatal: missing in_regs.json or out_si_ctrl.bin in $case_dir" >&2
  exit 2
fi

read_vars() {
  python3 - <<'PY' "$in_regs" "$out_si"
import json,struct,sys
in_regs_path=sys.argv[1]
out_si_path=sys.argv[2]

XYNTSC=[(263-17,2),(15,18),(30,9),(44,6),(52,5),(65,4),(87,3),(87,3),(87,3),(131,2),(131,2),(131,2)]
XYPAL=[(313-17,2),(15,21),(29,11),(45,7),(52,6),(63,5),(78,4),(104,3),(104,3),(104,3),(104,3),(156,2)]

in_regs=json.load(open(in_regs_path,"r"))
msec=int(in_regs["args"]["r3"],16) if "args" in in_regs and "r3" in in_regs["args"] else 0
raw=open(out_si_path,"rb").read()
poll=struct.unpack(">I", raw[4:8])[0]
x=(poll>>16)&0x3ff
y=(poll>>8)&0xff

msec_eff=msec if msec<=11 else 11

def match(table, tv):
    line,count=table[msec_eff]
    if y!=count:
        return None
    if x==line:
        return (tv,0,line,count,msec_eff)  # factor=1 => vi54 bit0=0
    if x==2*line:
        return (tv,1,line,count,msec_eff)  # factor=2 => vi54 bit0=1
    return None

res = match(XYNTSC, 0) or match(XYPAL, 1)
if res is None:
    # Still emit something useful for debugging
    print(f"MSEC={msec} MSEC_EFF={msec_eff} X={x} Y={y} MATCH=0")
    sys.exit(3)

tv, vi54_bit, line, count, msec_eff = res
vi54 = vi54_bit  # we only care about bit0 here

# Outputs we expect sdk_port to record:
expected_rate=msec_eff
expected_line=x
expected_count=y

print(f"MSEC={msec} MSEC_EFF={msec_eff} TV_FORMAT={tv} VI54={vi54} EXPECT_RATE={expected_rate} EXPECT_LINE={expected_line} EXPECT_COUNT={expected_count}")
PY
}

vars="$(read_vars)"
if [[ -z "$vars" ]]; then
  echo "fatal: could not derive expected vars" >&2
  exit 2
fi

eval "$vars"

scenario="$repo_root/tests/sdk/si/si_set_sampling_rate/host/si_set_sampling_rate_rvz_trace_replay_001_scenario.c"

expected_dir="$repo_root/tests/sdk/si/si_set_sampling_rate/expected"
actual_dir="$repo_root/tests/sdk/si/si_set_sampling_rate/actual"
mkdir -p "$expected_dir" "$actual_dir"

expected_bin="$expected_dir/si_set_sampling_rate_rvz_trace_${case_id}.bin"
actual_bin="$actual_dir/si_set_sampling_rate_rvz_trace_${case_id}.bin"

python3 - <<'PY' "$expected_bin" "$EXPECT_RATE" "$EXPECT_LINE" "$EXPECT_COUNT" "$VI54" "$TV_FORMAT" "$MSEC"
import sys,struct
out=sys.argv[1]
rate=int(sys.argv[2])
line=int(sys.argv[3])
count=int(sys.argv[4])
vi54=int(sys.argv[5])
tv=int(sys.argv[6])
msec=int(sys.argv[7])

def be32(x): return struct.pack(">I", x & 0xFFFFFFFF)

# Mirror the scenario layout exactly (0x40 bytes, but we only care about first 0x2C).
b=b"".join([
  be32(0xDEADBEEF),
  be32(rate),
  be32(line),
  be32(count),
  be32(1),          # setxy_calls
  be32(1),          # ints_enabled (restored)
  be32(1),          # disable_calls
  be32(1),          # restore_calls
  be32(vi54),
  be32(tv),
  be32(msec),
])
b=b.ljust(0x40, b"\x00")
open(out,"wb").write(b)
PY

GC_TRACE_CASE_ID="$case_id" \
GC_SEED_TV_FORMAT="$TV_FORMAT" \
GC_SEED_VI54="$VI54" \
GC_SEED_MSEC="$MSEC_EFF" \
  "$repo_root/tools/run_host_scenario.sh" "$scenario" >/dev/null

if [[ ! -f "$actual_bin" ]]; then
  echo "fatal: host run did not produce $actual_bin" >&2
  exit 2
fi

"$repo_root/tools/diff_bins.sh" "$expected_bin" "$actual_bin"

echo "[ok] $case_id (msec=$MSEC_EFF tv=$TV_FORMAT vi54=$VI54 line=$EXPECT_LINE count=$EXPECT_COUNT)"
