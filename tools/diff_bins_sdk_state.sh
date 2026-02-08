#!/usr/bin/env bash
set -euo pipefail

# Compare only the RAM-backed sdk_port state page inside MEM1 dumps.
#
# For MEM1 dumps (base 0x80000000):
#   GC_SDK_STATE_BASE = 0x817FE000 => file offset 0x017FE000 (size 0x2000)
#
# Usage:
#   tools/diff_bins_sdk_state.sh <expected_mem1.bin> <actual_mem1.bin>

EXP=${1:?expected bin required}
ACT=${2:?actual bin required}

python3 tools/ram_compare.py "$EXP" "$ACT" \
  --include-range 0x017FE000,0x2000
