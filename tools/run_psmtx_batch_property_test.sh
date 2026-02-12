#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
build_dir="$repo_root/tests/build/psmtx_batch_property"
mkdir -p "$build_dir"

cc="${CC:-cc}"
cflags=(
  -std=c11
  -O2
  -Wall
  -Wextra
  -I"$repo_root/tests/sdk/mtx/property"
  -I"$repo_root/src/sdk_port/mtx"
)

src_test="$repo_root/tests/sdk/mtx/property/psmtx_batch_property_test.c"
out_bin="$build_dir/psmtx_batch_property_test"

echo "[psmtx-batch-property-build] CC=$cc"
"$cc" "${cflags[@]}" \
  "$repo_root/src/sdk_port/mtx/mtx.c" \
  "$repo_root/src/sdk_port/mtx/vec.c" \
  "$repo_root/src/sdk_port/mtx/quat.c" \
  "$repo_root/src/sdk_port/mtx/mtx44.c" \
  "$src_test" -lm -o "$out_bin"

echo "[psmtx-batch-property-build] OK -> $out_bin"
"$out_bin" "$@"
