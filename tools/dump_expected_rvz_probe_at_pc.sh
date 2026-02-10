#!/usr/bin/env bash
set -euo pipefail

# Dump a small set of RVZ RAM regions at a PC checkpoint (breakpoint preferred).
#
# Purpose:
# - Real-game sanity checks without comparing full MEM1 images (host has no DOL loader yet).
# - Probe a few stable SDK globals from symbols.txt.
#
# Usage:
#   tools/dump_expected_rvz_probe_at_pc.sh <rvz_or_iso> <pc_addr_hex> <out_dir> [timeout_seconds] [hit_count]
#
# Output:
# - Writes one .bin per region under out_dir/
# - Writes manifest.sha256 with hashes + region metadata

EXEC_PATH=${1:?rvz_or_iso required}
PC_ADDR=${2:?pc_addr_hex required}
OUT_DIR=${3:?out_dir required}
TIMEOUT=${4:-30}
HIT_COUNT=${5:-1}
DELAY=${DOLPHIN_START_DELAY:-2}

repo_root="$(cd "$(dirname "$0")/.." && pwd)"

if [[ "$EXEC_PATH" != /* ]]; then
  EXEC_PATH="$repo_root/$EXEC_PATH"
fi

if [[ "$OUT_DIR" != /* ]]; then
  OUT_DIR="$repo_root/$OUT_DIR"
fi

mkdir -p "$OUT_DIR"

# Regions are chosen from MP4 symbols.txt (GMPE01_00):
# - __OSCurrHeap = 0x801D38B8
# - __OSArenaLo  = 0x801D38C0
# - __OSArenaHi  = 0x801D42F8
# - __PADSpec    = 0x801D450C
# - __PADFixBits = 0x801D4620
# - SamplingRate = 0x801D4628
#
# We dump a few aligned windows around those addresses so the probe remains useful
# even if we later add more nearby symbols.
declare -a NAMES ADDRS SIZES
NAMES+=("os_heap_arena_window")
ADDRS+=("0x801D38A0")
SIZES+=("0x80")

NAMES+=("os_arena_hi_window")
ADDRS+=("0x801D42E0")
SIZES+=("0x40")

NAMES+=("pad_si_window")
ADDRS+=("0x801D44F0")
SIZES+=("0x180")

# BootInfo window (OS_BASE_CACHED = 0x80000000). We want arenaLo/arenaHi values.
NAMES+=("bootinfo_window")
ADDRS+=("0x80000020")
SIZES+=("0x40")

# RVZ runs tend to be less tolerant of big GDB packets.
CHUNK=0x1000

manifest="$OUT_DIR/manifest.sha256"
: >"$manifest"

for i in "${!NAMES[@]}"; do
  name="${NAMES[$i]}"
  addr="${ADDRS[$i]}"
  size="${SIZES[$i]}"
  out_bin="$OUT_DIR/${name}.bin"

  echo "[rvz-probe] $name addr=$addr size=$size -> $out_bin"

  rc=1
  for attempt in 1 2 3; do
    # Kill stale headless Dolphin instances bound to the GDB port.
    pkill -f "Dolphin -b -d" >/dev/null 2>&1 || true

    PYTHONUNBUFFERED=1 python3 -u tools/ram_dump.py \
      --exec "$EXEC_PATH" \
      --breakpoint "$PC_ADDR" \
      --bp-timeout "$TIMEOUT" \
      --bp-hit-count "$HIT_COUNT" \
      --addr "$addr" \
      --size "$size" \
      --out "$out_bin" \
      --chunk "$CHUNK" \
      --enable-mmu \
      --delay "$DELAY"
    rc=$?

    if [[ $rc -eq 0 ]]; then
      break
    fi
    if [[ $rc -eq 3 ]]; then
      echo "[rvz-probe] breakpoint unsupported; falling back to PC polling" >&2
      PYTHONUNBUFFERED=1 python3 -u tools/ram_dump.py \
        --exec "$EXEC_PATH" \
        --pc-breakpoint "$PC_ADDR" \
        --pc-timeout "$TIMEOUT" \
        --pc-hit-count "$HIT_COUNT" \
        --addr "$addr" \
        --size "$size" \
        --out "$out_bin" \
        --chunk "$CHUNK" \
        --enable-mmu \
        --delay "$DELAY"
      rc=$?
      break
    fi

    echo "[rvz-probe] attempt $attempt failed (rc=$rc); retrying..." >&2
    sleep 1
  done

  if [[ $rc -ne 0 ]]; then
    exit $rc
  fi

  # Hash entry
  sha=$(shasum -a 256 "$out_bin" | awk '{print $1}')
  echo "$sha  $(basename "$out_bin")  addr=$addr size=$size pc=$PC_ADDR" >>"$manifest"
done

echo "[rvz-probe] wrote $manifest"

# Also emit a human-readable values file for quick comparisons.
python3 - <<'PY' "$OUT_DIR/bootinfo_window.bin" "$OUT_DIR/os_heap_arena_window.bin" "$OUT_DIR/os_arena_hi_window.bin" "$OUT_DIR/pad_si_window.bin" >"$OUT_DIR/values.txt"
import struct
import sys

boot = open(sys.argv[1], "rb").read()
os_heap = open(sys.argv[2], "rb").read()
os_ahi = open(sys.argv[3], "rb").read()
pad_si = open(sys.argv[4], "rb").read()

def be_u32(buf, off):
    return struct.unpack(">I", buf[off:off+4])[0]

# bootinfo_window base 0x80000020
boot_arena_lo = be_u32(boot, 0x30 - 0x20)
boot_arena_hi = be_u32(boot, 0x34 - 0x20)

# os_heap_arena_window base 0x801D38A0
__OSCurrHeap = be_u32(os_heap, 0x801D38B8 - 0x801D38A0)
__OSArenaLo  = be_u32(os_heap, 0x801D38C0 - 0x801D38A0)

# os_arena_hi_window base 0x801D42E0
__OSArenaHi  = be_u32(os_ahi, 0x801D42F8 - 0x801D42E0)

# pad_si_window base 0x801D44F0
__PADSpec    = be_u32(pad_si, 0x801D450C - 0x801D44F0)
__PADFixBits = be_u32(pad_si, 0x801D4620 - 0x801D44F0)
SamplingRate = be_u32(pad_si, 0x801D4628 - 0x801D44F0)

print("bootinfo:")
print(f"  arenaLo=0x{boot_arena_lo:08X}")
print(f"  arenaHi=0x{boot_arena_hi:08X}")
print("symbols:")
print(f"  __OSCurrHeap=0x{__OSCurrHeap:08X}")
print(f"  __OSArenaLo =0x{__OSArenaLo:08X}")
print(f"  __OSArenaHi =0x{__OSArenaHi:08X}")
print(f"  __PADSpec   =0x{__PADSpec:08X}")
print(f"  __PADFixBits=0x{__PADFixBits:08X}")
print(f"  SamplingRate=0x{SamplingRate:08X}")
PY

echo "[rvz-probe] wrote $OUT_DIR/values.txt"
