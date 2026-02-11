#!/usr/bin/env bash
set -euo pipefail

# Property-style parity test runner for MTX/VEC/QUAT.
#
# Builds a single host binary that contains BOTH:
# - Oracle: decomp C_* functions with oracle_ prefix (in mtx_oracle.h)
# - Port:   sdk_port C_* functions (mtx.c, vec.c, quat.c, mtx44.c)
#
# No -m32 needed: pure float math, no pointer-size dependencies.
#
# Usage:
#   tools/run_mtx_property_test.sh [--op=NAME] [--seed=N] [--num-runs=N] [-v]
#
# Examples:
#   tools/run_mtx_property_test.sh                        # 500 random seeds
#   tools/run_mtx_property_test.sh --seed=42              # single seed
#   tools/run_mtx_property_test.sh --op=VEC -v            # VEC only, verbose
#   tools/run_mtx_property_test.sh --num-runs=2000        # more seeds

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
build_dir="$repo_root/tests/build/mtx_property"
test_src="$repo_root/tests/sdk/mtx/property"
port_src="$repo_root/src/sdk_port/mtx"

mkdir -p "$build_dir"

# --- Collect pass-through args ---
args=()
opt_flags=(-O1 -g)

for arg in "$@"; do
    case "$arg" in
        -O*) opt_flags=("$arg") ;;
        *)   args+=("$arg") ;;
    esac
done

# --- Linker dead-strip flags (platform-dependent) ---
ld_gc_flags=()
case "$(uname -s)" in
    Darwin) ld_gc_flags+=(-Wl,-dead_strip) ;;
    *)      ld_gc_flags+=(-Wl,--gc-sections) ;;
esac

# --- Find compiler ---
CC="${CC:-}"
if [[ -z "$CC" ]]; then
    for try in cc clang gcc; do
        if command -v "$try" >/dev/null 2>&1; then CC="$try"; break; fi
    done
fi
if [[ -z "$CC" ]]; then
    # MSYS2 / Windows fallback
    for try in "/c/Program Files/LLVM/bin/clang" "/mingw64/bin/gcc"; do
        if [[ -x "$try" ]]; then CC="$try"; break; fi
    done
fi
if [[ -z "$CC" ]]; then
    echo "ERROR: no C compiler found. Set CC= or install clang/gcc."
    exit 2
fi

echo "[mtx-property-build] CC=$CC"
"$CC" "${opt_flags[@]}" -ffunction-sections -fdata-sections \
  -D_XOPEN_SOURCE=700 -D_CRT_SECURE_NO_WARNINGS \
  -Wno-implicit-function-declaration \
  -I"$test_src" \
  -I"$port_src" \
  "$test_src/mtx_property_test.c" \
  "$port_src/mtx.c" \
  "$port_src/vec.c" \
  "$port_src/quat.c" \
  "$port_src/mtx44.c" \
  "${ld_gc_flags[@]}" \
  -o "$build_dir/mtx_property_test"

echo "[mtx-property-build] OK -> $build_dir/mtx_property_test"
echo ""
"$build_dir/mtx_property_test" "${args[@]}"
