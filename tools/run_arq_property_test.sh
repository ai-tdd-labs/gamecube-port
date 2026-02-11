#!/usr/bin/env bash
set -euo pipefail

# Property-style parity test runner for ARQ (ARAM Request Queue).
#
# Builds a single host binary that contains BOTH:
# - Oracle: decomp ARQ functions (native structs) in arq_oracle.h
# - Port:   sdk_port ARQ (big-endian gc_mem) in arq.c
#
# Usage:
#   tools/run_arq_property_test.sh [--seed=N] [--num-runs=N] [--op=NAME] [-v]

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
build_dir="$repo_root/tests/build/arq_property"
test_src="$repo_root/tests/sdk/ar/property"
port_src="$repo_root/src/sdk_port/ar"
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

# --- Find compiler ---
CC="${CC:-}"
if [[ -z "$CC" ]]; then
    for try in clang gcc cc; do
        if command -v "$try" &>/dev/null; then CC="$try"; break; fi
    done
fi
if [[ -z "$CC" ]]; then
    for try in "/c/Program Files/LLVM/bin/clang" "/mingw64/bin/gcc"; do
        if [[ -x "$try" ]]; then CC="$try"; break; fi
    done
fi
if [[ -z "$CC" ]]; then
    echo "ERROR: no C compiler found. Set CC= or install clang/gcc."
    exit 2
fi

echo "[arq-property-build] CC=$CC"
"$CC" "${opt_flags[@]}" -ffunction-sections -fdata-sections \
  -D_XOPEN_SOURCE=700 -D_CRT_SECURE_NO_WARNINGS \
  -Wno-implicit-function-declaration \
  -I"$test_src" \
  -I"$port_src" \
  -I"$gc_mem_src" \
  "$test_src/arq_property_test.c" \
  "$port_src/arq.c" \
  "$gc_mem_src/gc_mem.c" \
  -o "$build_dir/arq_property_test"

echo "[arq-property-build] OK -> $build_dir/arq_property_test"
echo ""
"$build_dir/arq_property_test" "${args[@]}"
