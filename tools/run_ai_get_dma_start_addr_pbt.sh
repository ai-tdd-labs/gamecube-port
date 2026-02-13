#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
dol_dir="$repo_root/tests/sdk/ai/ai_get_dma_start_addr/dol/pbt/ai_get_dma_start_addr_pbt_001"
dol_elf="$dol_dir/ai_get_dma_start_addr_pbt_001.dol"
expected_bin="$repo_root/tests/sdk/ai/ai_get_dma_start_addr/expected/ai_get_dma_start_addr_pbt_001.bin"
actual_bin="$repo_root/tests/sdk/ai/ai_get_dma_start_addr/actual/ai_get_dma_start_addr_pbt_001.bin"
host_scenario="$repo_root/tests/sdk/ai/ai_get_dma_start_addr/host/ai_get_dma_start_addr_pbt_001_scenario.c"
dump_size=$((0x80))

make -C "$dol_dir" clean >/dev/null
make -C "$dol_dir" >/dev/null

"$repo_root/tools/dump_expected.sh" "$dol_elf" "$expected_bin" 0x80300000 "$dump_size" 0.7 >/dev/null
"$repo_root/tools/run_host_scenario.sh" "$host_scenario" >/dev/null
"$repo_root/tools/diff_bins.sh" "$expected_bin" "$actual_bin" >/dev/null

echo "PASS: AIGetDMAStartAddr pbt_001"

