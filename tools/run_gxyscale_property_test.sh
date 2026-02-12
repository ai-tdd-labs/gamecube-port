#!/usr/bin/env bash
set -euo pipefail

# Property-style parity test runner for GXGetYScaleFactor.
#
# Usage:
#   tools/run_gxyscale_property_test.sh [--seed=N] [--num-runs=N] [-v]

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
build_dir="$repo_root/tests/build/gxyscale_property"
test_src="$repo_root/tests/sdk/gx/property"

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
    for try in "/c/Program Files/LLVM/bin/clang" "/mingw64/bin/gcc"; do
        if [[ -x "$try" ]]; then CC="$try"; break; fi
    done
fi
if [[ -z "$CC" ]]; then
    echo "ERROR: no C compiler found."
    exit 2
fi

echo "[gxyscale-property-build] CC=$CC"
"$CC" "${opt_flags[@]}" -ffunction-sections -fdata-sections \
  -D_XOPEN_SOURCE=700 -D_CRT_SECURE_NO_WARNINGS \
  -Wno-implicit-function-declaration \
  -I"$repo_root/src/sdk_port/gx" \
  -I"$repo_root/src/sdk_port" \
  "$test_src/gxyscale_property_test.c" \
  "$repo_root/src/sdk_port/gx/GX.c" \
  "$repo_root/src/sdk_port/gc_mem.c" \
  "${ld_gc_flags[@]}" \
  -o "$build_dir/gxyscale_property_test"

echo "[gxyscale-property-build] OK -> $build_dir/gxyscale_property_test"
echo ""
"$build_dir/gxyscale_property_test" "${args[@]}"
