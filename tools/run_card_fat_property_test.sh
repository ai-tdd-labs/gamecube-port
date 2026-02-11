#!/usr/bin/env bash
set -euo pipefail

# Property-style parity test runner for CARD FAT block allocator.
#
# Builds a single host binary that contains BOTH:
# - Oracle: decomp __CARDCheckSum/__CARDAllocBlock/__CARDFreeBlock (native u16)
# - Port:   sdk_port card_fat.c (big-endian u16 in gc_mem)
#
# Usage:
#   tools/run_card_fat_property_test.sh [--seed=N] [--num-runs=N] [--op=NAME] [-v]

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
build_dir="$repo_root/tests/build/card_fat_property"
test_src="$repo_root/tests/sdk/card/property"
port_src="$repo_root/src/sdk_port/card"
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

echo "[card-fat-property-build] CC=$CC"
"$CC" "${opt_flags[@]}" -ffunction-sections -fdata-sections \
  -D_XOPEN_SOURCE=700 -D_CRT_SECURE_NO_WARNINGS \
  -Wno-implicit-function-declaration \
  -I"$test_src" \
  -I"$port_src" \
  -I"$gc_mem_src" \
  "$test_src/card_fat_property_test.c" \
  "$port_src/card_fat.c" \
  "$gc_mem_src/gc_mem.c" \
  "${ld_gc_flags[@]}" \
  -o "$build_dir/card_fat_property_test"

echo "[card-fat-property-build] OK -> $build_dir/card_fat_property_test"
echo ""
"$build_dir/card_fat_property_test" "${args[@]}"
