#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
build_dir="$repo_root/tests/build/property"
mkdir -p "$build_dir"

exe="$build_dir/dvdfs_property_test"

opt_flags=(-O2 -g0)
if [[ "${GC_HOST_DEBUG:-0}" == "1" ]]; then
  opt_flags=(-O0 -g)
fi

ld_gc_flags=()
case "$(uname -s)" in
  Darwin) ld_gc_flags+=(-Wl,-dead_strip) ;;
  *)      ld_gc_flags+=(-Wl,--gc-sections) ;;
esac

CC="${CC:-}"
if [[ -z "$CC" ]]; then
  for try in cc gcc clang; do
    if command -v "$try" >/dev/null 2>&1; then
      CC="$try"
      break
    fi
  done
fi
if [[ -z "$CC" ]]; then
  echo "fatal: no C compiler found (set CC=)" >&2
  exit 2
fi

echo "[property-build] dvdfs_property_test (CC=$CC)"
"$CC" "${opt_flags[@]}" -ffunction-sections -fdata-sections \
  -D_XOPEN_SOURCE=700 -D_CRT_SECURE_NO_WARNINGS \
  -I"$repo_root/tests" \
  "$repo_root/tests/sdk/dvd/dvdfs/property/dvdfs_property_test.c" \
  "${ld_gc_flags[@]}" \
  -o "$exe"

echo "[property-run]  $exe $*"
"$exe" "$@"
