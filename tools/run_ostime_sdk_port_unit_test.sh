#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
build_dir="$repo_root/tests/build/ostime_sdk_port_unit"
mkdir -p "$build_dir"

exe="$build_dir/ostime_sdk_port_unit_test"

cc -O2 -g0 -ffunction-sections -fdata-sections \
  -I"$repo_root/src" \
  "$repo_root/src/sdk_port/os/OSRtc.c" \
  "$repo_root/tests/sdk/os/ostime/property/ostime_sdk_port_unit_test.c" \
  -Wl,-dead_strip \
  -o "$exe"

"$exe"
