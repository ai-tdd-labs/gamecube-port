#!/usr/bin/env bash
set -euo pipefail

# Build+run one host testcase and produce actual.bin.
#
# Usage:
#   tools/run_host_test.sh <tests/sdk/.../host/<case>_host.c>
#
# The host binary writes the actual.bin into the function's actual/ folder.

SRC=${1:?host test .c path required}

repo_root="$(cd "$(dirname "$0")/.." && pwd)"

if [[ "$SRC" != /* ]]; then
  SRC="$repo_root/$SRC"
fi

test_dir="$(cd "$(dirname "$SRC")" && pwd)"
exe="$test_dir/$(basename "$SRC" .c)"

echo "[host-build] $SRC"
cc -O2 -g \
  -I"$repo_root/tests" \
  -I"$repo_root/tests/harness" \
  -I"$repo_root/src" \
  -I"$repo_root/src/sdk_port" \
  "$repo_root/tests/harness/gc_host_ram.c" \
  "$repo_root/src/sdk_port/os/OSArena.c" \
  "$SRC" \
  -o "$exe"

echo "[host-run] $exe"
(cd "$test_dir" && "$exe")

