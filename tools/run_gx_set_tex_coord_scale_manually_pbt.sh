#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
dol_dir="$repo_root/tests/sdk/gx/gx_set_tex_coord_scale_manually/dol/pbt/gx_set_tex_coord_scale_manually_pbt_001"
dol_elf="$dol_dir/gx_set_tex_coord_scale_manually_pbt_001.dol"
expected_bin="$repo_root/tests/sdk/gx/gx_set_tex_coord_scale_manually/expected/gx_set_tex_coord_scale_manually_pbt_001.bin"
actual_bin="$repo_root/tests/sdk/gx/gx_set_tex_coord_scale_manually/actual/gx_set_tex_coord_scale_manually_pbt_001.bin"
host_scenario="$repo_root/tests/sdk/gx/gx_set_tex_coord_scale_manually/host/gx_set_tex_coord_scale_manually_pbt_001_scenario.c"
dump_size=$((0xA0))

make -C "$dol_dir" clean >/dev/null
make -C "$dol_dir" >/dev/null

"$repo_root/tools/dump_expected.sh" "$dol_elf" "$expected_bin" 0x80300000 "$dump_size" 0.7 >/dev/null
"$repo_root/tools/run_host_scenario.sh" "$host_scenario" >/dev/null
"$repo_root/tools/diff_bins.sh" "$expected_bin" "$actual_bin" >/dev/null

echo "PASS: GXSetTexCoordScaleManually pbt_001"
