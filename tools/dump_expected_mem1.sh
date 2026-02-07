#!/usr/bin/env bash
set -euo pipefail

DOL_PATH=${1:?dol_path required}
OUT_BIN=${2:?out_bin required}
RUN=${3:-0.75}

# MEM1 = 24 MiB @ 0x80000000
ADDR=0x80000000
SIZE=0x01800000

exec tools/dump_expected.sh "$DOL_PATH" "$OUT_BIN" "$ADDR" "$SIZE" "$RUN" 0x10000
# Use a larger chunk size for MEM1 dumps: 24 MiB / 0x10000 = 384 reads.
# 0x1000 would require 6144 reads and is much more brittle/slow over GDB.
