#!/usr/bin/env bash
set -euo pipefail

# Property-style parity test runner for OSThread scheduler.
#
# Builds a single host binary that contains BOTH:
# - Oracle: decomp scheduler functions (native structs) in osthread_oracle.h
# - Port:   sdk_port scheduler (big-endian gc_mem) in osthread.c
#
# Usage:
#   tools/run_osthread_property_test.sh [--seed=N] [--num-runs=N] [--op=NAME] [-v]

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
build_dir="$repo_root/tests/build/osthread_property"
test_src="$repo_root/tests/sdk/os/osthread/property"
port_src="$repo_root/src/sdk_port/os"
gc_mem_src="$repo_root/src/sdk_port"

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

echo "[osthread-property-build] CC=$CC"
"$CC" "${opt_flags[@]}" -ffunction-sections -fdata-sections \
  -D_XOPEN_SOURCE=700 -D_CRT_SECURE_NO_WARNINGS \
  -Wno-implicit-function-declaration \
  -I"$test_src" \
  -I"$port_src" \
  -I"$gc_mem_src" \
  "$test_src/osthread_property_test.c" \
  "$port_src/osthread.c" \
  "$gc_mem_src/gc_mem.c" \
  "${ld_gc_flags[@]}" \
  -o "$build_dir/osthread_property_test"

echo "[osthread-property-build] OK -> $build_dir/osthread_property_test"
echo ""
"$build_dir/osthread_property_test" "${args[@]}"
