#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
build_dir="$repo_root/tests/build/dvdcancel_unit"
mkdir -p "$build_dir"

exe="$build_dir/dvdcancel_unit_test"

cc -O2 -g0 -ffunction-sections -fdata-sections \
  -I"$repo_root/src" \
  "$repo_root/src/sdk_port/gc_mem.c" \
  "$repo_root/src/sdk_port/dvd/DVD.c" \
  "$repo_root/tests/sdk/dvd/property/dvdcancel_unit_test.c" \
  -Wl,-dead_strip \
  -o "$exe"

"$exe"
