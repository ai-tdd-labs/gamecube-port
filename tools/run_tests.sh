#!/usr/bin/env bash
set -euo pipefail

# Canonical test runner for this repo (tests/sdk).
#
# Expected flow per testcase:
# 1) Build DOL(s) under tests/sdk/<subsystem>/<func>/dol/<case>/
# 2) Dump expected.bin via Dolphin into tests/sdk/<subsystem>/<func>/expected/
# 3) Later: produce actual.bin via our SDK port and diff.
#
# Usage:
#   tools/run_tests.sh build
#   tools/run_tests.sh expected
#   tools/run_tests.sh all
#   tools/run_tests.sh build   tests/sdk/os/os_set_arena_lo
#   tools/run_tests.sh expected tests/sdk/os/os_set_arena_lo
#
# Environment:
#   ADDR, SIZE, RUN_SECONDS override Dolphin RAM dump region.

MODE=${1:-all}
FILTER_ROOT=${2:-}
ADDR=${ADDR:-0x80300000}
SIZE=${SIZE:-0x40}
RUN_SECONDS=${RUN_SECONDS:-0.5}

repo_root="$(cd "$(dirname "$0")/.." && pwd)"

tests_root="$repo_root/tests/sdk"
if [[ -n "$FILTER_ROOT" ]]; then
  # Allow passing either an absolute path or repo-relative.
  if [[ "$FILTER_ROOT" != /* ]]; then
    FILTER_ROOT="$repo_root/$FILTER_ROOT"
  fi
  tests_root="$FILTER_ROOT"
fi

build_all() {
  # Build every DOL testcase we can find.
  # Support either dol/<case>/Makefile or dol/<game>/<case>/Makefile.
  find "$tests_root" -type f -name Makefile \( -path "*/dol/*/Makefile" -o -path "*/dol/*/*/Makefile" \) -print0 |
    while IFS= read -r -d '' mk; do
      d="$(dirname "$mk")"
      echo "[build] $d"
      (cd "$d" && make)
    done
}

dump_expected_all() {
  # Find every produced .dol under dol/ and dump expected into expected/<name>.bin.
  find "$tests_root" -type f -name "*.dol" -path "*/dol/*" -print0 |
    while IFS= read -r -d '' dol; do
      case_dir="$(dirname "$dol")"

      # Walk up to find the "dol" directory, then one more up is the function dir.
      p="$case_dir"
      while [[ "$(basename "$p")" != "dol" ]]; do
        p="$(dirname "$p")"
      done
      func_dir="$(dirname "$p")" # tests/sdk/<subsystem>/<func>

      exp_dir="$func_dir/expected"
      mkdir -p "$exp_dir"

      base="$(basename "$dol" .dol)"
      out="$exp_dir/$base.bin"
      echo "[expected] $dol -> $out"
      "$repo_root/tools/dump_expected.sh" "$dol" "$out" "$ADDR" "$SIZE" "$RUN_SECONDS"
    done
}

case "$MODE" in
  build) build_all ;;
  expected) dump_expected_all ;;
  all)
    build_all
    dump_expected_all
    ;;
  *)
    echo "Unknown mode: $MODE" >&2
    echo "Usage: tools/run_tests.sh [build|expected|all] [optional path filter]" >&2
    exit 2
    ;;
esac
