#!/usr/bin/env bash
set -euo pipefail

# Replay one retail RVZ PADReset trace case against sdk_port.
#
# Usage:
#   tools/replay_trace_case_pad_reset.sh <trace_case_dir>

case_dir=${1:?trace case directory required}

repo_root="$(cd "$(dirname "$0")/.." && pwd)"

if [[ "$case_dir" != /* ]]; then
  case_dir="$repo_root/$case_dir"
fi

in_regs="$case_dir/in_regs.json"
out_regs="$case_dir/out_regs.json"
in_resetting_bits="$case_dir/in_resetting_bits.bin"
in_resetting_chan="$case_dir/in_resetting_chan.bin"
in_recal_bits="$case_dir/in_recal_bits.bin"
in_reset_cb="$case_dir/in_reset_cb.bin"
in_pad_type="$case_dir/in_pad_type.bin"

out_resetting_bits="$case_dir/out_resetting_bits.bin"
out_resetting_chan="$case_dir/out_resetting_chan.bin"
out_recal_bits="$case_dir/out_recal_bits.bin"
out_reset_cb="$case_dir/out_reset_cb.bin"
out_pad_type="$case_dir/out_pad_type.bin"

case_id="$(basename "$case_dir")"

for f in "$in_regs" "$out_regs" \
  "$in_resetting_bits" "$in_resetting_chan" "$in_recal_bits" "$in_reset_cb" "$in_pad_type" \
  "$out_resetting_bits" "$out_resetting_chan" "$out_recal_bits" "$out_reset_cb" "$out_pad_type"; do
  if [[ ! -f "$f" ]]; then
    echo "fatal: missing required file: $f" >&2
    exit 2
  fi
done

vars="$(python3 - <<'PY' "$in_regs" "$out_regs"
import json,sys
jin=json.load(open(sys.argv[1],"r"))
jout=json.load(open(sys.argv[2],"r"))
mask=int(jin["args"]["r3"],16)
ret=int(jout["args"]["r3"],16)
print(f"MASK=0x{mask:08X}")
print(f"RET={ret}")
PY
)"
eval "$vars"

scenario="$repo_root/tests/sdk/pad/pad_reset/host/pad_reset_rvz_trace_replay_001_scenario.c"

expected_dir="$repo_root/tests/sdk/pad/pad_reset/expected"
actual_dir="$repo_root/tests/sdk/pad/pad_reset/actual"
mkdir -p "$expected_dir" "$actual_dir"

expected_bin="$expected_dir/pad_reset_rvz_trace_${case_id}.bin"
actual_bin="$actual_dir/pad_reset_rvz_trace_${case_id}.bin"

python3 - <<'PY' "$expected_bin" "$RET" \
  "$out_resetting_bits" "$out_resetting_chan" "$out_recal_bits" "$out_reset_cb" "$out_pad_type"
import sys,struct
out=sys.argv[1]
ret=int(sys.argv[2])
paths=sys.argv[3:]

rb=open(paths[0],'rb').read()
rc=open(paths[1],'rb').read()
recal=open(paths[2],'rb').read()
rcb=open(paths[3],'rb').read()
ptype=open(paths[4],'rb').read()
if len(rb)!=4 or len(rc)!=4 or len(recal)!=4 or len(rcb)!=4 or len(ptype)!=0x20:
  raise SystemExit("unexpected dump sizes")

def be32(x): return struct.pack(">I", x & 0xFFFFFFFF)
b = bytearray(b"\x00"*0x40)
b[0x00:0x04]=be32(0xDEADBEEF)
b[0x04:0x08]=be32(ret)
b[0x10:0x14]=rb
b[0x14:0x18]=rc
b[0x18:0x1C]=recal
b[0x1C:0x20]=rcb
b[0x20:0x40]=ptype
open(out,"wb").write(b)
PY

GC_TRACE_CASE_ID="$case_id" \
GC_TRACE_MASK="$MASK" \
GC_TRACE_IN_RESETTING_BITS="$in_resetting_bits" \
GC_TRACE_IN_RESETTING_CHAN="$in_resetting_chan" \
GC_TRACE_IN_RECAL_BITS="$in_recal_bits" \
GC_TRACE_IN_RESET_CB="$in_reset_cb" \
GC_TRACE_IN_PAD_TYPE="$in_pad_type" \
  "$repo_root/tools/run_host_scenario.sh" "$scenario" >/dev/null

if [[ ! -f "$actual_bin" ]]; then
  echo "fatal: host run did not produce $actual_bin" >&2
  exit 2
fi

"$repo_root/tools/diff_bins.sh" "$expected_bin" "$actual_bin"
echo "[ok] $case_id (mask=$MASK ret=$RET)"
