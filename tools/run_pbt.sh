#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"

# Usage:
#   tools/run_pbt.sh [suite] [iters] [seed]
# Suites:
#   - os_round_32b (default)
#   - mtx_core
#   - dvd_core

suite="${1:-os_round_32b}"
iters="${2:-200000}"
seed="${3:-0xC0DEC0DE}"

if [[ "$suite" =~ ^[0-9]+$ ]]; then
  # Backward compatibility with old positional form: run_pbt.sh <iters> <seed>
  seed="${2:-0xC0DEC0DE}"
  iters="$suite"
  suite="os_round_32b"
fi

build_dir="$repo_root/tests/build/pbt"
mkdir -p "$build_dir"

case "$suite" in
  os_round_32b)
    cc -O2 -g0 \
      -I"$repo_root/src" -I"$repo_root/src/sdk_port" \
      "$repo_root/src/sdk_port/gc_mem.c" \
      "$repo_root/src/sdk_port/os/OSArena.c" \
      "$repo_root/tests/pbt/os/os_round_32b/os_round_32b_pbt.c" \
      -o "$build_dir/os_round_32b_pbt"
    "$build_dir/os_round_32b_pbt" "$iters" "$seed"
    ;;
  mtx_core)
    cc -O2 -g0 \
      -I"$repo_root/src" -I"$repo_root/src/sdk_port" \
      "$repo_root/src/sdk_port/mtx/mtx.c" \
      "$repo_root/src/sdk_port/mtx/mtx44.c" \
      "$repo_root/tests/pbt/mtx/mtx_core_pbt.c" \
      -lm \
      -o "$build_dir/mtx_core_pbt"
    "$build_dir/mtx_core_pbt" "$iters" "$seed"
    ;;
  dvd_core)
    cc -O2 -g0 \
      -I"$repo_root/src" -I"$repo_root/src/sdk_port" \
      "$repo_root/src/sdk_port/gc_mem.c" \
      "$repo_root/src/sdk_port/dvd/DVD.c" \
      "$repo_root/tests/pbt/dvd/dvd_core_pbt.c" \
      -o "$build_dir/dvd_core_pbt"
    "$build_dir/dvd_core_pbt" "$iters" "$seed"
    ;;
  *)
    echo "Unknown PBT suite: $suite" >&2
    exit 2
    ;;
esac
