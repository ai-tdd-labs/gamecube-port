#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$repo_root"

echo "[dualcheck] strict-vs-adapted oracle checks"
echo "[dualcheck] card checksum (strict decomp vs adapted oracle vs sdk_port)"
tools/run_card_fat_property_test.sh --op=CHECKSUM --num-runs="${1:-500}"
echo "[dualcheck] mtx core (strict decomp leaf oracle vs sdk_port)"
tools/run_pbt.sh mtx_core "${1:-500}" 0xC0DEC0DE
echo "[dualcheck] os_round_32b (strict decomp leaf oracle vs sdk_port)"
tools/run_pbt.sh os_round_32b "${1:-500}" 0xC0DEC0DE
echo "[dualcheck] dvd_core (strict decomp leaf oracle vs sdk_port)"
tools/run_pbt.sh dvd_core "${1:-500}" 0xC0DEC0DE

echo "[dualcheck] baseline adapted-oracle suites"
tools/run_arq_property_test.sh --num-runs=50
tools/run_card_fat_property_test.sh --num-runs=50

echo "[dualcheck] PASS"
