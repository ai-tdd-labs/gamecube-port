#!/usr/bin/env bash
set -euo pipefail

# Build+run one host smoke scenario (may span multiple subsystems).
#
# Usage:
#   tools/run_host_smoke.sh <tests/sdk/smoke/.../host/<scenario>.c>
#
# The scenario is a full program with main() and is responsible for dumping
# to tests/sdk/smoke/.../actual/*.bin.

SCENARIO_SRC=${1:?scenario .c path required}

repo_root="$(cd "$(dirname "$0")/.." && pwd)"

if [[ "$SCENARIO_SRC" != /* ]]; then
  SCENARIO_SRC="$repo_root/$SCENARIO_SRC"
fi

scenario_dir="$(cd "$(dirname "$SCENARIO_SRC")" && pwd)"
scenario_base="$(basename "$SCENARIO_SRC" .c)"
exe="$scenario_dir/${scenario_base}_host"

echo "[host-smoke-build] $SCENARIO_SRC"
cc -O2 -g \
  -I"$repo_root/tests" \
  -I"$repo_root/tests/harness" \
  -I"$repo_root/src" \
  -I"$repo_root/src/sdk_port" \
  "$repo_root/tests/harness/gc_host_ram.c" \
  "$repo_root/src/sdk_port/gc_mem.c" \
  "$repo_root/src/sdk_port/os/OSArena.c" \
  "$repo_root/src/sdk_port/os/OSCache.c" \
  "$repo_root/src/sdk_port/os/OSAlloc.c" \
  "$repo_root/src/sdk_port/os/OSInterrupts.c" \
  "$repo_root/src/sdk_port/os/OSInit.c" \
  "$repo_root/src/sdk_port/os/OSFastCast.c" \
  "$repo_root/src/sdk_port/os/OSError.c" \
  "$repo_root/src/sdk_port/os/OSSystem.c" \
  "$repo_root/src/sdk_port/os/OSRtc.c" \
  "$repo_root/src/sdk_port/si/SI.c" \
  "$repo_root/src/sdk_port/vi/VI.c" \
  "$repo_root/src/sdk_port/pad/PAD.c" \
  "$repo_root/src/sdk_port/gx/GX.c" \
  "$repo_root/src/sdk_port/dvd/DVD.c" \
  "$SCENARIO_SRC" \
  -o "$exe"

echo "[host-smoke-run] $exe"
(cd "$scenario_dir" && GC_REPO_ROOT="$repo_root" "$exe")
