#!/usr/bin/env bash
set -euo pipefail

# Compare only a small deterministic window inside a full MEM1 dump.
# This is used by smoke tests that dump the whole 24 MiB for debugging,
# but only assert bit-exact equivalence for a snapshot/marker region.
#
# Usage:
#   tools/diff_bins_smoke.sh <expected_mem1.bin> <actual_mem1.bin>

EXP=${1:?expected bin required}
ACT=${2:?actual bin required}

# For MEM1 dumps (base 0x80000000):
# 0x80300000 marker + 0x80300100 snapshot live at file offsets 0x00300000+.
#
# We now also compare the RAM-backed sdk_port state page at the end of MEM1:
#   base 0x817F0000 => file offset 0x017F0000 (size 0x10000)
python3 tools/ram_compare.py "$EXP" "$ACT" \
  --include-range 0x00300000,0x2000 \
  --include-range 0x017F0000,0x10000
