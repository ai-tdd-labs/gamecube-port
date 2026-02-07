#!/usr/bin/env bash
set -euo pipefail

DOL_PATH=${1:?dol_path required}
OUT_BIN=${2:?out_bin required}
RUN=${3:-0.75}

# MEM1 = 24 MiB @ 0x80000000
ADDR=0x80000000
SIZE=0x01800000

# NOTE: Larger chunk sizes (e.g. 0x10000) can cause Dolphin's GDB stub to drop
# the connection mid-dump on some builds. Keep this conservative and let
# ram_dump.py opportunistically increase if it proves stable.
exec tools/dump_expected.sh "$DOL_PATH" "$OUT_BIN" "$ADDR" "$SIZE" "$RUN" 0x1000
