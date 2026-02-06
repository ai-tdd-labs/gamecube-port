#!/usr/bin/env bash
set -euo pipefail

SCENARIO=${1:?scenario_c required}
OUT_BIN=${2:?out_bin required}

# Scenario is expected to write its dump to a fixed path inside tests/.
# This wrapper just runs it and copies to the requested output path.

tmp_out="tests/sdk/smoke/mp4_init_chain_001/actual/mp4_init_chain_001_mem1.bin"

tools/run_host_smoke.sh "$SCENARIO"

if [ ! -f "$tmp_out" ]; then
  echo "[dump_actual_mem1] missing output: $tmp_out" >&2
  exit 1
fi

cp -f "$tmp_out" "$OUT_BIN"
