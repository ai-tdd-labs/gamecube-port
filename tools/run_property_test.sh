#!/usr/bin/env bash
set -euo pipefail

# Property-style parity test runner for OSAlloc (portable offsets oracle).
#
# Builds a single host binary that contains BOTH:
# - Oracle: derived from decomp OSAlloc, but using 32-bit offset pointers
# - Port:   sdk_port OSAlloc (emulated big-endian via gc_mem)
#
# Usage:
#   tools/run_property_test.sh [--op=NAME] [--seed=N] [--num-runs=N] [--steps=N] [-v]
#
# Examples:
#   tools/run_property_test.sh                         # default runs
#   tools/run_property_test.sh --seed=12345            # single seed
#   tools/run_property_test.sh --op=DLInsert -v        # focused suite
#   tools/run_property_test.sh --num-runs=2000 -v      # verbose + more runs

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
build_dir="$repo_root/tests/build/property"
mkdir -p "$build_dir"

exe="$build_dir/osalloc_property_test"

# Probe whether this toolchain can actually build 32-bit (-m32) binaries.
# On modern macOS (especially Apple Silicon), 32-bit targets are generally unsupported.
supports_m32() {
  local tmp_dir tmp_c tmp_out
  tmp_dir="$(mktemp -d 2>/dev/null || mktemp -d -t gc-m32-probe)"
  tmp_c="$tmp_dir/probe.c"
  tmp_out="$tmp_dir/probe"
  cat >"$tmp_c" <<'EOF'
int main(void) { return 0; }
EOF
  if "$CC" -m32 "$tmp_c" -o "$tmp_out" >/dev/null 2>&1; then
    rm -rf "$tmp_dir"
    return 0
  fi
  rm -rf "$tmp_dir"
  return 1
}

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

echo "[property-build] osalloc_property_test (CC=$CC)"
"$CC" "${opt_flags[@]}" -ffunction-sections -fdata-sections \
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
