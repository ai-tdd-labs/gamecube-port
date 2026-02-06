#!/usr/bin/env bash
set -euo pipefail

# Compare two RAM dumps and show PASS/FAIL.
# Usage:
#   tools/diff_bins.sh <expected_bin> <actual_bin>

EXP=${1:?expected bin required}
ACT=${2:?actual bin required}

# Ignore low-memory prefix where Dolphin/loader can differ (exception vectors, bootinfo, misc).
# For MEM1 dumps at 0x80000000, 0x00004000 covers 0x00000000-0x00003FFF.
python3 tools/ram_compare.py "$EXP" "$ACT" --ignore-prefix 0x4000
