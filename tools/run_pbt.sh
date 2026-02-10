#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"

iters="${1:-200000}"
seed="${2:-0xC0DEC0DE}"

build_dir="$repo_root/tests/build/pbt"
mkdir -p "$build_dir"

cc -O2 -g0 \
  -I"$repo_root/src" -I"$repo_root/src/sdk_port" \
  "$repo_root/src/sdk_port/gc_mem.c" \
  "$repo_root/src/sdk_port/os/OSArena.c" \
  "$repo_root/tests/pbt/os/os_round_32b/os_round_32b_pbt.c" \
  -o "$build_dir/os_round_32b_pbt"

"$build_dir/os_round_32b_pbt" "$iters" "$seed"
