#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
dol_dir="$repo_root/tests/sdk/card/card_unmount/dol/pbt/card_unmount_pbt_001"
dol_dol="$dol_dir/card_unmount_pbt_001.dol"
expected_bin="$repo_root/tests/sdk/card/card_unmount/expected/card_unmount_pbt_001.bin"
actual_bin="$repo_root/tests/sdk/card/card_unmount/actual/card_unmount_pbt_001.bin"
host_scenario="$repo_root/tests/sdk/card/card_unmount/host/card_unmount_pbt_001_scenario.c"
dump_size=$((0x100))

mkdir -p "$(dirname "$expected_bin")" "$(dirname "$actual_bin")"

make -C "$dol_dir" clean >/dev/null
make -C "$dol_dir" >/dev/null

"$repo_root/tools/dump_expected.sh" "$dol_dol" "$expected_bin" 0x80300000 "$dump_size" 0.9 >/dev/null
"$repo_root/tools/run_host_scenario.sh" "$host_scenario" >/dev/null
"$repo_root/tools/diff_bins.sh" "$expected_bin" "$actual_bin" >/dev/null

echo "PASS: CARDUnmount pbt_001"
