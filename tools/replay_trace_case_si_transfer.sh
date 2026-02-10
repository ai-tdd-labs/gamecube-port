#!/usr/bin/env bash
set -euo pipefail

# Safety: refuse to run if the worktree is dirty.
if ! git diff --quiet || ! git diff --cached --quiet; then
  echo "fatal: git worktree has uncommitted changes; commit/stash first" >&2
  git status --porcelain=v1 >&2 || true
  exit 2
fi


# Replay one retail RVZ SITransfer trace case against sdk_port and assert the
# observed outputs (derived from the trace) match our implementation.
#
# Usage:
#   tools/replay_trace_case_si_transfer.sh <trace_case_dir>

case_dir=${1:?trace case directory required}

repo_root="$(cd "$(dirname "$0")/.." && pwd)"

if [[ "$case_dir" != /* ]]; then
  case_dir="$repo_root/$case_dir"
fi

case_id="$(basename "$case_dir")"

in_regs="$case_dir/in_regs.json"
out_regs="$case_dir/out_regs.json"
in_outbuf="$case_dir/in_outbuf.bin"
in_type="$case_dir/in_type_data.bin"
in_si="$case_dir/in_si_core.bin"
in_inputbuf="$case_dir/in_inputbuf.bin"

out_outbuf="$case_dir/out_outbuf.bin"
out_type="$case_dir/out_type_data.bin"
out_si="$case_dir/out_si_core.bin"
out_inputbuf="$case_dir/out_inputbuf.bin"

for f in "$in_regs" "$out_regs" "$in_outbuf" "$in_type" "$in_si" "$in_inputbuf" "$out_outbuf" "$out_type" "$out_si" "$out_inputbuf"; do
  if [[ ! -f "$f" ]]; then
    echo "fatal: missing $f" >&2
    exit 2
  fi
done

read_vars() {
  python3 - "$in_regs" "$in_si" "$out_si" <<'PY'
import json,struct,sys
in_regs_path=sys.argv[1]
in_si_path=sys.argv[2]
out_si_path=sys.argv[3]

regs=json.load(open(in_regs_path,"r"))
args=regs.get("args",{})
def u32(k, default=0):
  v=args.get(k)
  if v is None: return default
  return int(v,16) & 0xFFFFFFFF

chan=u32("r3")
out_ptr=u32("r4")
out_bytes=u32("r5")
in_ptr=u32("r6")
in_bytes=u32("r7")
cb_ptr=u32("r8")
delay_hi=u32("r9")
delay_lo=u32("r10")
delay=((delay_hi<<32)|delay_lo) & 0xFFFFFFFFFFFFFFFF

# Derive a deterministic system_time seed for scheduling behavior.
#
# In retail (SIBios.c):
#   now = __OSGetSystemTime()
#   fire = (delay==0) ? now : (XferTime[chan] + delay)
#   if now < fire: OSSetAlarm(...), and Alarm[chan].fire becomes `fire`.
#
# For replay we don't need the true absolute time, just whether `now < fire`.
# Use the retail output to infer whether an alarm was armed and pick:
# - if alarm armed: now = fire - 1
# - else:           now = fire
out_si=open(out_si_path,"rb").read()
in_si=open(in_si_path,"rb").read()
alarm_base=0x80 + chan*0x28
alarm_handler=struct.unpack(">I", out_si[alarm_base+0x00:alarm_base+0x04])[0]
alarm_fire_hi=struct.unpack(">I", out_si[alarm_base+0x08:alarm_base+0x0C])[0]
alarm_fire_lo=struct.unpack(">I", out_si[alarm_base+0x0C:alarm_base+0x10])[0]
alarm_fire=((alarm_fire_hi<<32)|alarm_fire_lo) & 0xFFFFFFFFFFFFFFFF

pkt_base=chan*0x20
pkt_chan=struct.unpack(">I", out_si[pkt_base+0x00:pkt_base+0x04])[0]
xfer_off=0x140 + chan*8
xfer_hi=struct.unpack(">I", in_si[xfer_off+0:xfer_off+4])[0]
xfer_lo=struct.unpack(">I", in_si[xfer_off+4:xfer_off+8])[0]
xfer=((xfer_hi<<32)|xfer_lo) & 0xFFFFFFFFFFFFFFFF

fire = xfer + delay if delay != 0 else 0

if alarm_handler != 0 and fire != 0:
  sys_time = (fire - 1) & 0xFFFFFFFFFFFFFFFF
else:
  sys_time = fire

delta = 0
if alarm_handler != 0 and alarm_fire != 0 and alarm_fire >= fire:
  delta = alarm_fire - fire

# If packet->chan stays -1 and no alarm is armed, retail succeeded immediately
# via __SITransfer() and returned TRUE without queuing a packet.
hw_ok = 1 if (pkt_chan == 0xFFFFFFFF and alarm_handler == 0) else 0

print(f"GC_TRACE_CHAN={chan}")
print(f"GC_TRACE_OUT_PTR=0x{out_ptr:08X}")
print(f"GC_TRACE_OUT_BYTES={out_bytes}")
print(f"GC_TRACE_IN_PTR=0x{in_ptr:08X}")
print(f"GC_TRACE_IN_BYTES={in_bytes}")
print(f"GC_TRACE_CB_PTR=0x{cb_ptr:08X}")
print(f"GC_TRACE_DELAY64=0x{delay:016X}")
print(f"GC_TRACE_SYSTEM_TIME_SEED=0x{sys_time:016X}")
print(f"GC_TRACE_OSSETALARM_DELTA={delta}")
print(f"GC_TRACE_HW_XFER_OK={hw_ok}")
PY
}

vars="$(read_vars)"

eval "$vars"

scenario="$repo_root/tests/sdk/si/si_transfer/host/si_transfer_rvz_trace_replay_001_scenario.c"

expected_dir="$repo_root/tests/sdk/si/si_transfer/expected"
actual_dir="$repo_root/tests/sdk/si/si_transfer/actual"
mkdir -p "$expected_dir" "$actual_dir"

expected_bin="$expected_dir/si_transfer_rvz_trace_${case_id}.bin"
actual_bin="$actual_dir/si_transfer_rvz_trace_${case_id}.bin"

python3 - <<'PY' "$expected_bin" "$out_outbuf" "$out_type" "$out_si" "$out_inputbuf" "$GC_TRACE_CHAN" "$GC_TRACE_OUT_BYTES" "$GC_TRACE_IN_BYTES" "$GC_TRACE_DELAY64"
import sys,struct
out_path=sys.argv[1]
out_outbuf_path=sys.argv[2]
out_type_path=sys.argv[3]
out_si_path=sys.argv[4]
out_inputbuf_path=sys.argv[5]
chan=int(sys.argv[6],0)
out_bytes=int(sys.argv[7],0)
in_bytes=int(sys.argv[8],0)
delay=int(sys.argv[9],0)

def be32(x): return struct.pack(">I", x & 0xFFFFFFFF)

out_outbuf=open(out_outbuf_path,'rb').read()
out_type=open(out_type_path,'rb').read()
out_si=open(out_si_path,'rb').read()
out_inputbuf=open(out_inputbuf_path,'rb').read()
assert len(out_outbuf)==0x20
assert len(out_type)==0x10
assert len(out_si)==0x160
assert len(out_inputbuf)==0x40

b=b"".join([
  be32(0xDEADBEEF),
  be32(1),              # SITransfer returns TRUE in observed MP4 cases
  be32(chan),
  be32(out_bytes),
  be32(in_bytes),
  be32((delay>>32)&0xFFFFFFFF),
  be32(delay&0xFFFFFFFF),
])
b=b.ljust(0x20, b"\x00")
b += out_outbuf
b += out_type
b += out_si
b += out_inputbuf
assert len(b)==0x1F0
open(out_path,"wb").write(b)
PY

GC_TRACE_CASE_ID="$case_id" \
GC_TRACE_IN_OUTBUF="$in_outbuf" \
GC_TRACE_IN_TYPE_DATA="$in_type" \
GC_TRACE_IN_SI_CORE="$in_si" \
GC_TRACE_IN_INPUTBUF="$in_inputbuf" \
GC_TRACE_CHAN="$GC_TRACE_CHAN" \
GC_TRACE_OUT_PTR="$GC_TRACE_OUT_PTR" \
GC_TRACE_OUT_BYTES="$GC_TRACE_OUT_BYTES" \
GC_TRACE_IN_PTR="$GC_TRACE_IN_PTR" \
GC_TRACE_IN_BYTES="$GC_TRACE_IN_BYTES" \
GC_TRACE_CB_PTR="$GC_TRACE_CB_PTR" \
GC_TRACE_DELAY64="$GC_TRACE_DELAY64" \
GC_TRACE_SYSTEM_TIME_SEED="$GC_TRACE_SYSTEM_TIME_SEED" \
GC_TRACE_OSSETALARM_DELTA="$GC_TRACE_OSSETALARM_DELTA" \
GC_TRACE_HW_XFER_OK="$GC_TRACE_HW_XFER_OK" \
GC_HOST_MAIN_DUMP_ADDR=0x80300000 \
GC_HOST_MAIN_DUMP_SIZE=0x1F0 \
  "$repo_root/tools/run_host_scenario.sh" "$scenario" >/dev/null

if [[ ! -f "$actual_bin" ]]; then
  echo "fatal: host run did not produce $actual_bin" >&2
  exit 2
fi

"$repo_root/tools/diff_bins.sh" "$expected_bin" "$actual_bin"
echo "[ok] $case_id"
