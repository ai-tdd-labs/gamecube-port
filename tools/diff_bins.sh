#!/usr/bin/env bash
set -euo pipefail

# Compare two RAM dumps and show PASS/FAIL.
# Usage:
#   tools/diff_bins.sh <expected_bin> <actual_bin>

EXP=${1:?expected bin required}
ACT=${2:?actual bin required}

python3 tools/ram_compare.py "$EXP" "$ACT"
