#!/usr/bin/env bash
set -euo pipefail

# Dump a small set of host RAM regions after running a host scenario.
#
# This mirrors tools/dump_expected_rvz_probe_at_pc.sh, but uses our host virtual RAM
# instead of Dolphin. This is useful for RVZ-vs-host sanity checks at a checkpoint
# (without requiring full MEM1 bit-compare).
#
# Usage:
#   tools/dump_actual_host_probe_at_scenario.sh <scenario_c> <out_dir>
#
# Output:
# - Writes one .bin per region under out_dir/
# - Writes manifest.sha256 with hashes + region metadata
#
# Notes:
# - The scenario is run once while dumping the full 32 MiB host RAM buffer.
# - Region bins are sliced out of that buffer.

SCENARIO=${1:?scenario_c required}
OUT_DIR=${2:?out_dir required}

repo_root="$(cd "$(dirname "$0")/.." && pwd)"

if [[ "$SCENARIO" != /* ]]; then
  SCENARIO="$repo_root/$SCENARIO"
fi
if [[ "$OUT_DIR" != /* ]]; then
  OUT_DIR="$repo_root/$OUT_DIR"
fi

mkdir -p "$OUT_DIR"

ram32="$OUT_DIR/ram32.bin"
manifest="$OUT_DIR/manifest.sha256"
: >"$manifest"

# Run once and dump the entire host RAM buffer (32 MiB at 0x80000000).
GC_HOST_DUMP_ADDR=0x80000000 \
GC_HOST_DUMP_SIZE=0x02000000 \
GC_HOST_DUMP_PATH="$ram32" \
tools/run_host_scenario.sh "$SCENARIO"

declare -a NAMES ADDRS SIZES
# BootInfo window (OS_BASE_CACHED = 0x80000000). We care about arenaLo/arenaHi.
NAMES+=("bootinfo_window")
ADDRS+=("0x80000020")
SIZES+=("0x40")

# Our SDK globals do not live at the retail .bss addresses on host; they live in
# the RAM-backed sdk_state page. Dump a small window around it so we can parse
# and compare semantic values to RVZ probes.
#
# GC_SDK_STATE_BASE comes from src/sdk_port/sdk_state.h.
NAMES+=("sdk_state_window")
ADDRS+=("0x817FE000")
SIZES+=("0x400")

export GC_HOST_PROBE_NAMES
export GC_HOST_PROBE_ADDRS
export GC_HOST_PROBE_SIZES
GC_HOST_PROBE_NAMES="$(IFS=,; echo "${NAMES[*]}")"
GC_HOST_PROBE_ADDRS="$(IFS=,; echo "${ADDRS[*]}")"
GC_HOST_PROBE_SIZES="$(IFS=,; echo "${SIZES[*]}")"

python3 - <<'PY' "$ram32" "$OUT_DIR" "$manifest"
import os
import sys

ram_path = sys.argv[1]
out_dir = sys.argv[2]
manifest = sys.argv[3]

names = os.environ["GC_HOST_PROBE_NAMES"].split(",")
addrs = os.environ["GC_HOST_PROBE_ADDRS"].split(",")
sizes = os.environ["GC_HOST_PROBE_SIZES"].split(",")

base = 0x80000000
ram = open(ram_path, "rb").read()

def parse_int(s: str) -> int:
    return int(s, 0)

for name, addr_s, size_s in zip(names, addrs, sizes):
    addr = parse_int(addr_s)
    size = parse_int(size_s)
    off = addr - base
    if off < 0 or off + size > len(ram):
        raise SystemExit(f"region out of range: {name} addr={addr_s} size={size_s}")
    out_bin = os.path.join(out_dir, f"{name}.bin")
    with open(out_bin, "wb") as f:
        f.write(ram[off : off + size])
    # sha256
    import hashlib

    sha = hashlib.sha256(open(out_bin, "rb").read()).hexdigest()
    with open(manifest, "a", encoding="utf-8") as m:
        m.write(f"{sha}  {os.path.basename(out_bin)}  addr={addr_s} size={size_s} scenario={os.path.basename(out_dir)}\n")

print(f"[host-probe] wrote {manifest}")
PY

echo "[host-probe] wrote $manifest"

# Also emit a human-readable values file for quick comparisons.
python3 - <<'PY' "$OUT_DIR/bootinfo_window.bin" "$OUT_DIR/sdk_state_window.bin" >"$OUT_DIR/values.txt"
import struct
import sys

boot = open(sys.argv[1], "rb").read()
sdk = open(sys.argv[2], "rb").read()

def be_u32(buf, off):
    return struct.unpack(">I", buf[off:off+4])[0]

# bootinfo_window base 0x80000020
boot_arena_lo = be_u32(boot, 0x30 - 0x20)
boot_arena_hi = be_u32(boot, 0x34 - 0x20)

# sdk_state_window base 0x817FE000
SDK_MAGIC = be_u32(sdk, 0x00)
os_arena_lo = be_u32(sdk, 0x10)
os_arena_hi = be_u32(sdk, 0x14)
os_curr_heap = be_u32(sdk, 0x20)
si_sampling_rate = be_u32(sdk, 0x360)
pad_initialized = be_u32(sdk, 0x320)
pad_spec = be_u32(sdk, 0x324)

print("bootinfo:")
print(f"  arenaLo=0x{boot_arena_lo:08X}")
print(f"  arenaHi=0x{boot_arena_hi:08X}")
print("sdk_state:")
print(f"  magic=0x{SDK_MAGIC:08X}")
print(f"  os_arena_lo=0x{os_arena_lo:08X}")
print(f"  os_arena_hi=0x{os_arena_hi:08X}")
print(f"  os_curr_heap=0x{os_curr_heap:08X}")
print(f"  si_sampling_rate=0x{si_sampling_rate:08X}")
print(f"  pad_initialized=0x{pad_initialized:08X}")
print(f"  pad_spec=0x{pad_spec:08X}")
PY

echo "[host-probe] wrote $OUT_DIR/values.txt"
