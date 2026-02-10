#!/usr/bin/env bash
set -euo pipefail

# Dump MEM1 (24 MiB @ 0x80000000) from a real MP4 RVZ/ISO at a PC checkpoint.
#
# Usage:
#   tools/dump_expected_rvz_mem1_at_pc.sh <rvz_or_iso> <pc_addr_hex> <out_bin> [timeout_seconds] [hit_count]
#
# Notes:
# - Prefer a real breakpoint (Z0/Z1). If the stub doesn't support it, fall back to --pc-breakpoint polling.
# - Adds --enable-mmu best-effort to avoid "Invalid read/write ... enable MMU" warnings.

EXEC_PATH=${1:?rvz_or_iso required}
PC_ADDR=${2:?pc_addr_hex required}
OUT_BIN=${3:?out_bin required}
TIMEOUT=${4:-30}
HIT_COUNT=${5:-1}

ADDR=0x80000000
SIZE=0x01800000
# RVZ runs tend to be less tolerant of very large GDB memory packets.
# Start conservative; ram_dump.py will shrink further if needed.
CHUNK=0x1000

repo_root="$(cd "$(dirname "$0")/.." && pwd)"

if [[ "$EXEC_PATH" != /* ]]; then
  EXEC_PATH="$repo_root/$EXEC_PATH"
fi

if [[ "$OUT_BIN" != /* ]]; then
  OUT_BIN="$repo_root/$OUT_BIN"
fi

mkdir -p "$(dirname "$OUT_BIN")"

# Kill stale headless Dolphin instances bound to the GDB port.
pkill -f "Dolphin -b -d" >/dev/null 2>&1 || true

PYTHONUNBUFFERED=1 python3 -u tools/ram_dump.py \
  --exec "$EXEC_PATH" \
  --breakpoint "$PC_ADDR" \
  --bp-timeout "$TIMEOUT" \
  --bp-hit-count "$HIT_COUNT" \
  --addr "$ADDR" \
  --size "$SIZE" \
  --out "$OUT_BIN" \
  --chunk "$CHUNK" \
  --enable-mmu \
  && exit 0

rc=$?
if [[ $rc -ne 3 ]]; then
  exit $rc
fi

echo "[dump_expected_rvz_mem1_at_pc] breakpoint unsupported; falling back to PC polling" >&2
PYTHONUNBUFFERED=1 python3 -u tools/ram_dump.py \
  --exec "$EXEC_PATH" \
  --pc-breakpoint "$PC_ADDR" \
  --pc-timeout "$TIMEOUT" \
  --pc-hit-count "$HIT_COUNT" \
  --addr "$ADDR" \
  --size "$SIZE" \
  --out "$OUT_BIN" \
  --chunk "$CHUNK" \
  --enable-mmu
