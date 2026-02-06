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
#
# Environment:
#   ADDR, SIZE, RUN_SECONDS override Dolphin RAM dump region.

MODE=${1:-all}
ADDR=${ADDR:-0x80300000}
SIZE=${SIZE:-0x40}
RUN_SECONDS=${RUN_SECONDS:-0.5}

repo_root="$(cd "$(dirname "$0")/.." && pwd)"

build_all() {
  # Build every DOL testcase we can find.
  find "$repo_root/tests/sdk" -type f -name Makefile -path "*/dol/*/Makefile" -print0 |
    while IFS= read -r -d '' mk; do
      d="$(dirname "$mk")"
      echo "[build] $d"
      (cd "$d" && make)
    done
}

dump_expected_all() {
  # For each dol/<case>/, find the produced .dol and dump expected into expected/<name>.bin.
  find "$repo_root/tests/sdk" -type d -path "*/dol/*" -print0 |
    while IFS= read -r -d '' d; do
      dol="$(ls -1 "$d"/*.dol 2>/dev/null | head -n 1 || true)"
      if [[ -z "${dol}" ]]; then
        continue
      fi

      func_dir="$(cd "$d/../.." && pwd)" # tests/sdk/<subsystem>/<func>
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
    echo "Usage: tools/run_tests.sh [build|expected|all]" >&2
    exit 2
    ;;
esac
