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

# Replay one retail RVZ SIGetResponse trace case against sdk_port and assert the
# observed outputs (derived from the trace) match our implementation.
#
# Usage:
#   tools/replay_trace_case_si_get_response.sh <trace_case_dir>

case_dir=${1:?trace case directory required}

repo_root="$(cd "$(dirname "$0")/.." && pwd)"

if [[ "$case_dir" != /* ]]; then
  case_dir="$repo_root/$case_dir"
fi

case_id="$(basename "$case_dir")"

in_regs="$case_dir/in_regs.json"
out_regs="$case_dir/out_regs.json"
in_data="$case_dir/in_data.bin"
out_data="$case_dir/out_data.bin"
in_valid="$case_dir/in_inputbuf_valid.bin"
out_valid="$case_dir/out_inputbuf_valid.bin"
in_buf="$case_dir/in_inputbuf.bin"
out_buf="$case_dir/out_inputbuf.bin"
in_vc="$case_dir/in_inputbuf_vcount.bin"
out_vc="$case_dir/out_inputbuf_vcount.bin"

for f in "$in_regs" "$out_regs" "$in_data" "$out_data" "$in_valid" "$out_valid" "$in_buf" "$out_buf" "$in_vc" "$out_vc"; do
  if [[ ! -f "$f" ]]; then
    echo "fatal: missing $f" >&2
    exit 2
  fi
done

read_vars() {
  python3 - "$in_regs" "$out_regs" "$out_buf" "$out_data" <<'PY'
import json,struct,sys
in_regs_path=sys.argv[1]
out_regs_path=sys.argv[2]
out_buf_path=sys.argv[3]
out_data_path=sys.argv[4]

in_regs=json.load(open(in_regs_path,"r"))
out_regs=json.load(open(out_regs_path,"r"))
args=in_regs.get("args",{})
out_args=out_regs.get("args",{})

def u32_hex(d,k,default=0):
  v=d.get(k)
  if v is None: return default
  return int(v,16) & 0xFFFFFFFF

chan=u32_hex(args,"r3")
data_ptr=u32_hex(args,"r4")
ret=u32_hex(out_args,"r3")

# Retail SIGetResponse returns BOOL; treat any nonzero as 1.
ret = 1 if ret != 0 else 0

out_buf=open(out_buf_path,'rb').read()
out_data=open(out_data_path,'rb').read()
assert len(out_buf)==0x20
assert len(out_data)==0x08

# Seed behavior:
# - If retail returned 1, model SI_ERROR_RDST and seed response words from the
#   observed output bytes (prefer out_data, which is what the caller sees).
# - If retail returned 0, seed status=0 so SIGetResponseRaw does nothing.
if ret == 1:
  status=0x20  # SI_ERROR_RDST
  w0=struct.unpack(">I", out_data[0:4])[0]
  w1=struct.unpack(">I", out_data[4:8])[0]
else:
  status=0
  w0=struct.unpack(">I", out_buf[0:4])[0]
  w1=struct.unpack(">I", out_buf[4:8])[0]

print(f"GC_TRACE_CHAN={chan}")
print(f"GC_TRACE_DATA_PTR=0x{data_ptr:08X}")
print(f"GC_TRACE_RET={ret}")
print(f"GC_TRACE_SI_STATUS=0x{status:08X}")
print(f"GC_TRACE_SI_WORD0=0x{w0:08X}")
print(f"GC_TRACE_SI_WORD1=0x{w1:08X}")
PY
}

vars="$(read_vars)"
eval "$vars"

scenario="$repo_root/tests/sdk/si/si_get_response/host/si_get_response_rvz_trace_replay_001_scenario.c"

expected_dir="$repo_root/tests/sdk/si/si_get_response/expected"
actual_dir="$repo_root/tests/sdk/si/si_get_response/actual"
mkdir -p "$expected_dir" "$actual_dir"

expected_bin="$expected_dir/si_get_response_rvz_trace_${case_id}.bin"
actual_bin="$actual_dir/si_get_response_rvz_trace_${case_id}.bin"

python3 - <<'PY' "$expected_bin" "$out_regs" "$out_data" "$out_valid" "$out_buf" "$out_vc" "$GC_TRACE_CHAN" "$GC_TRACE_DATA_PTR"
import json,struct,sys
out_path=sys.argv[1]
out_regs_path=sys.argv[2]
out_data_path=sys.argv[3]
out_valid_path=sys.argv[4]
out_buf_path=sys.argv[5]
out_vc_path=sys.argv[6]
chan=int(sys.argv[7],0)
data_ptr=int(sys.argv[8],0)

out_regs=json.load(open(out_regs_path,'r'))
args=out_regs.get("args",{})
ret=int(args.get("r3","0"),16) if "r3" in args else 0
ret=1 if ret!=0 else 0

def be32(x): return struct.pack(">I", x & 0xFFFFFFFF)

out_data=open(out_data_path,'rb').read()
out_valid=open(out_valid_path,'rb').read()
out_buf=open(out_buf_path,'rb').read()
out_vc=open(out_vc_path,'rb').read()
assert len(out_data)==0x08
assert len(out_valid)==0x10
assert len(out_buf)==0x20
assert len(out_vc)==0x10

b=b"".join([
  be32(0xDEADBEEF),
  be32(ret),
  be32(chan),
  be32(data_ptr),
])
b=b.ljust(0x10, b"\x00")
b += out_data
b += out_valid
b += out_buf
b += out_vc
assert len(b)==0x58
open(out_path,"wb").write(b)
PY

GC_TRACE_CASE_ID="$case_id" \
GC_TRACE_IN_DATA="$in_data" \
GC_TRACE_IN_INPUTBUF_VALID="$in_valid" \
GC_TRACE_IN_INPUTBUF="$in_buf" \
GC_TRACE_IN_INPUTBUF_VCOUNT="$in_vc" \
GC_TRACE_CHAN="$GC_TRACE_CHAN" \
GC_TRACE_DATA_PTR="$GC_TRACE_DATA_PTR" \
GC_TRACE_SI_STATUS="$GC_TRACE_SI_STATUS" \
GC_TRACE_SI_WORD0="$GC_TRACE_SI_WORD0" \
GC_TRACE_SI_WORD1="$GC_TRACE_SI_WORD1" \
GC_HOST_MAIN_DUMP_ADDR=0x80300000 \
GC_HOST_MAIN_DUMP_SIZE=0x58 \
GC_HOST_DEBUG=0 \
tools/run_host_scenario.sh "$scenario"

"$repo_root/tools/diff_bins.sh" "$expected_bin" "$actual_bin"

echo "[ok] $case_id"
