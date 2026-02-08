#!/usr/bin/env bash
set -euo pipefail

SCENARIO=${1:?scenario_c required}
OUT_BIN=${2:?out_bin required}

# Produce a host-side MEM1 dump (0x80000000..0x81800000) by running a host scenario.
#
# This uses tests/harness/gc_host_runner.c extra-dump env vars, so it works for any
# scenario that uses the standard host runner (tools/run_host_scenario.sh).
#
# Usage:
#   tools/dump_actual_mem1.sh tests/workload/mp4/mp4_gwinit_001_scenario.c tests/actual/.../foo_mem1.bin

ADDR=0x80000000
SIZE=0x01800000

repo_root="$(cd "$(dirname "$0")/.." && pwd)"

if [[ "$OUT_BIN" != /* ]]; then
  OUT_BIN="$repo_root/$OUT_BIN"
fi

GC_HOST_DUMP_ADDR="$ADDR" \
GC_HOST_DUMP_SIZE="$SIZE" \
GC_HOST_DUMP_PATH="$OUT_BIN" \
tools/run_host_scenario.sh "$SCENARIO"
