#!/usr/bin/env bash
set -euo pipefail

# Property-style parity/invariant runner for DVDReadPrio.
#
# Usage:
#   tools/run_dvdreadprio_property_test.sh [--seed=N] [--num-runs=N] [--op=L0|L1|L2|L3|FULL|... ] [-v]

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
build_dir="$repo_root/tests/build/dvdreadprio_property"
test_src="$repo_root/tests/sdk/dvd/property"
dvd_src="$repo_root/src/sdk_port/dvd"
sdk_src="$repo_root/src/sdk_port"

mkdir -p "$build_dir"

args=()
opt_flags=(-O1 -g)
for arg in "$@"; do
    case "$arg" in
        -O*) opt_flags=("$arg") ;;
        *)   args+=("$arg") ;;
    esac
done

ld_gc_flags=()
case "$(uname -s)" in
    Darwin) ld_gc_flags+=(-Wl,-dead_strip) ;;
    *)      ld_gc_flags+=(-Wl,--gc-sections) ;;
esac

CC="${CC:-}"
if [[ -z "$CC" ]]; then
    for try in cc clang gcc; do
        if command -v "$try" >/dev/null 2>&1; then CC="$try"; break; fi
    done
fi
if [[ -z "$CC" ]]; then
    echo "ERROR: no C compiler found."
    exit 2
fi

echo "[dvdreadprio-property-build] CC=$CC"
"$CC" "${opt_flags[@]}" -ffunction-sections -fdata-sections \
  -D_XOPEN_SOURCE=700 -D_CRT_SECURE_NO_WARNINGS \
  -Wno-implicit-function-declaration \
  -I"$dvd_src" \
  -I"$sdk_src" \
  "$test_src/dvdreadprio_property_test.c" \
  "$dvd_src/DVD.c" \
  "$sdk_src/gc_mem.c" \
  "${ld_gc_flags[@]}" \
  -o "$build_dir/dvdreadprio_property_test"

echo "[dvdreadprio-property-build] OK -> $build_dir/dvdreadprio_property_test"
echo ""
"$build_dir/dvdreadprio_property_test" "${args[@]}"
