#!/usr/bin/env bash
set -euo pipefail

# Dump expected RAM region from running a DOL in Dolphin via GDB stub.
# Usage:
#   tools/dump_expected.sh <dol_path> <out_bin> [addr] [size] [run_seconds]

DOL_PATH=${1:?dol_path required}
OUT_BIN=${2:?out_bin required}
ADDR=${3:-0x80300000}
SIZE=${4:-0x40}
RUN=${5:-0.5}

python3 tools/ram_dump.py \
  --exec "$DOL_PATH" \
  --addr "$ADDR" \
  --size "$SIZE" \
  --out "$OUT_BIN" \
  --run "$RUN" \
  --halt
