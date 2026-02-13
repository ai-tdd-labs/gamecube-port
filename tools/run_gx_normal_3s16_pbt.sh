#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
dol_dir="$repo_root/tests/sdk/gx/gx_normal_3s16/dol/pbt/gx_normal_3s16_pbt_001"
dol_elf="$dol_dir/gx_normal_3s16_pbt_001.dol"
expected_bin="$repo_root/tests/sdk/gx/gx_normal_3s16/expected/gx_normal_3s16_pbt_001.bin"
actual_bin="$repo_root/tests/sdk/gx/gx_normal_3s16/actual/gx_normal_3s16_pbt_001.bin"
host_scenario="$repo_root/tests/sdk/gx/gx_normal_3s16/host/gx_normal_3s16_pbt_001_scenario.c"
dump_size=$((0x6C))

make -C "$dol_dir" clean >/dev/null
make -C "$dol_dir" >/dev/null

"$repo_root/tools/dump_expected.sh" "$dol_elf" "$expected_bin" 0x80300000 "$dump_size" 0.7 >/dev/null
"$repo_root/tools/run_host_scenario.sh" "$host_scenario" >/dev/null
"$repo_root/tools/diff_bins.sh" "$expected_bin" "$actual_bin" >/dev/null

echo "PASS: GXNormal3s16 pbt_001"
