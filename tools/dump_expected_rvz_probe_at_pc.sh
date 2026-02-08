#!/usr/bin/env bash
set -euo pipefail

# Dump a small set of RVZ RAM regions at a PC checkpoint (breakpoint preferred).
#
# Purpose:
# - Real-game sanity checks without comparing full MEM1 images (host has no DOL loader yet).
# - Probe a few stable SDK globals from symbols.txt.
#
# Usage:
#   tools/dump_expected_rvz_probe_at_pc.sh <rvz_or_iso> <pc_addr_hex> <out_dir> [timeout_seconds]
#
# Output:
# - Writes one .bin per region under out_dir/
# - Writes manifest.sha256 with hashes + region metadata

EXEC_PATH=${1:?rvz_or_iso required}
PC_ADDR=${2:?pc_addr_hex required}
OUT_DIR=${3:?out_dir required}
TIMEOUT=${4:-30}

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

  # Kill stale headless Dolphin instances bound to the GDB port.
  pkill -f "Dolphin -b -d" >/dev/null 2>&1 || true

  PYTHONUNBUFFERED=1 python3 -u tools/ram_dump.py \
    --exec "$EXEC_PATH" \
    --breakpoint "$PC_ADDR" \
    --bp-timeout "$TIMEOUT" \
    --addr "$addr" \
    --size "$size" \
    --out "$out_bin" \
    --chunk "$CHUNK" \
    --enable-mmu \
    && true

  rc=$?
  if [[ $rc -eq 3 ]]; then
    echo "[rvz-probe] breakpoint unsupported; falling back to PC polling" >&2
    PYTHONUNBUFFERED=1 python3 -u tools/ram_dump.py \
      --exec "$EXEC_PATH" \
      --pc-breakpoint "$PC_ADDR" \
      --pc-timeout "$TIMEOUT" \
      --addr "$addr" \
      --size "$size" \
      --out "$out_bin" \
      --chunk "$CHUNK" \
      --enable-mmu
  elif [[ $rc -ne 0 ]]; then
    exit $rc
  fi

  # Hash entry
  sha=$(shasum -a 256 "$out_bin" | awk '{print $1}')
  echo "$sha  $(basename "$out_bin")  addr=$addr size=$size pc=$PC_ADDR" >>"$manifest"
done

echo "[rvz-probe] wrote $manifest"

