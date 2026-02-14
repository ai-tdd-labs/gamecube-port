#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
dol_dir="$repo_root/tests/sdk/dvd/dvd_cancel/dol/generic/dvd_cancel_generic_001"
dol_elf="$dol_dir/dvd_cancel_generic_001.dol"
expected_bin="$repo_root/tests/sdk/dvd/dvd_cancel/expected/dvd_cancel_generic_001.bin"
actual_bin="$repo_root/tests/sdk/dvd/dvd_cancel/actual/dvd_cancel_generic_001.bin"
host_scenario="$repo_root/tests/sdk/dvd/dvd_cancel/host/dvd_cancel_generic_001_scenario.c"

mkdir -p "$(dirname "$expected_bin")" "$(dirname "$actual_bin")"

make -C "$dol_dir" clean >/dev/null
make -C "$dol_dir" >/dev/null

"$repo_root/tools/dump_expected.sh" "$dol_elf" "$expected_bin" 0x80300000 0x40 0.5 >/dev/null
"$repo_root/tools/run_host_scenario.sh" "$host_scenario" >/dev/null
"$repo_root/tools/diff_bins.sh" "$expected_bin" "$actual_bin" >/dev/null

echo "PASS: dvd_cancel/generic_001"
