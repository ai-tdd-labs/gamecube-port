#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
dol_dir="$repo_root/tests/sdk/card/exi_dma_card_proto/dol/pbt/exi_dma_card_proto_pbt_001"
dol_elf="$dol_dir/exi_dma_card_proto_pbt_001.dol"
expected_bin="$repo_root/tests/sdk/card/exi_dma_card_proto/expected/exi_dma_card_proto_pbt_001.bin"
actual_bin="$repo_root/tests/sdk/card/exi_dma_card_proto/actual/exi_dma_card_proto_pbt_001.bin"
host_scenario="$repo_root/tests/sdk/card/exi_dma_card_proto/host/exi_dma_card_proto_pbt_001_scenario.c"
dump_size=$((0x40))

mkdir -p "$(dirname "$expected_bin")" "$(dirname "$actual_bin")"

make -C "$dol_dir" clean >/dev/null
make -C "$dol_dir" >/dev/null

"$repo_root/tools/dump_expected.sh" "$dol_elf" "$expected_bin" 0x80300000 "$dump_size" 0.7 >/dev/null
"$repo_root/tools/run_host_scenario.sh" "$host_scenario" >/dev/null
"$repo_root/tools/diff_bins.sh" "$expected_bin" "$actual_bin" >/dev/null

echo "PASS: EXI DMA card proto pbt_001"

