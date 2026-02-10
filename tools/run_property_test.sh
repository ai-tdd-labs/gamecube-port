#!/usr/bin/env bash
set -euo pipefail

# Property-based test runner for OSAlloc (v2 — 32-bit oracle, leaf-first).
#
# Builds a single -m32 binary that contains BOTH the oracle (decomp, native
# 32-bit pointers, HeapDescs in arena) and the sdk_port (emulated big-endian
# via gc_mem).  Drives them with the same random seed and compares results.
#
# Requires -m32 support (32-bit libc).  With -m32:
#   sizeof(void*)==4, sizeof(long)==4, sizeof(Cell)==16, sizeof(HeapDesc)==12
#   — exactly matching the GC, no struct-size hacks needed.
#
# Usage:
#   tools/run_property_test.sh [--seed=N] [--op=NAME] [--num-runs=500] [-v]
#
# Examples:
#   tools/run_property_test.sh                         # 500 random seeds (full)
#   tools/run_property_test.sh --seed=12345            # single seed
#   tools/run_property_test.sh --op=DLInsert           # leaf DL test
#   tools/run_property_test.sh --op=OSAllocFromHeap    # specific API op
#   tools/run_property_test.sh --num-runs=2000 -v      # verbose + more seeds

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
build_dir="$repo_root/tests/build/property"
mkdir -p "$build_dir"

exe="$build_dir/osalloc_property_test"

# Debug mode: set GC_HOST_DEBUG=1 for -O0 -g.
opt_flags=(-O2 -g0)
if [[ "${GC_HOST_DEBUG:-0}" == "1" ]]; then
  opt_flags=(-O0 -g)
fi

# Linker dead-strip flags (platform-dependent).
ld_gc_flags=()
case "$(uname -s)" in
  Darwin) ld_gc_flags+=(-Wl,-dead_strip) ;;
  *)      ld_gc_flags+=(-Wl,--gc-sections) ;;
esac

# Find a C compiler.
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

echo "[property-build] osalloc_property_test -m32 (CC=$CC)"
"$CC" -m32 "${opt_flags[@]}" -ffunction-sections -fdata-sections \
  -D_XOPEN_SOURCE=700 -D_CRT_SECURE_NO_WARNINGS \
  -Wno-implicit-function-declaration \
  -I"$repo_root/tests" \
  -I"$repo_root/tests/harness" \
  -I"$repo_root/src" \
  -I"$repo_root/src/sdk_port" \
  "$repo_root/tests/harness/gc_host_ram.c" \
  "$repo_root/src/sdk_port/gc_mem.c" \
  "$repo_root/src/sdk_port/os/OSAlloc.c" \
  "$repo_root/src/sdk_port/os/OSArena.c" \
  "$repo_root/tests/sdk/os/os_alloc/property/osalloc_property_test.c" \
  "${ld_gc_flags[@]}" \
  -o "$exe"

echo "[property-run]  $exe $*"
"$exe" "$@"
