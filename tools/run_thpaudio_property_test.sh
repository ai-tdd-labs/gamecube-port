#!/usr/bin/env bash
set -euo pipefail

# Property-style parity test runner for THPAudioDecode.
#
# Usage:
#   tools/run_thpaudio_property_test.sh [--seed=N] [--num-runs=N] [-v]

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
build_dir="$repo_root/tests/build/thpaudio_property"
test_src="$repo_root/tests/sdk/thp/property"

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

echo "[thpaudio-property-build] CC=$CC"
"$CC" "${opt_flags[@]}" -ffunction-sections -fdata-sections \
  -D_XOPEN_SOURCE=700 -D_CRT_SECURE_NO_WARNINGS \
  -Wno-implicit-function-declaration \
  "$test_src/thpaudio_property_test.c" \
  "${ld_gc_flags[@]}" \
  -o "$build_dir/thpaudio_property_test"

echo "[thpaudio-property-build] OK -> $build_dir/thpaudio_property_test"
echo ""
"$build_dir/thpaudio_property_test" "${args[@]}"
