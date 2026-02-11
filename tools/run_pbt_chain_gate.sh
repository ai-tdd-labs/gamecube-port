#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$repo_root"

echo "[gate] PBT suites"
tools/run_pbt.sh os_round_32b 20000 0xC0DEC0DE
tools/run_pbt.sh mtx_core 50000 0xC0DEC0DE
tools/run_pbt.sh dvd_core 20000 0xC0DEC0DE

echo "[gate] ARQ/CARD-FAT property suites"
tools/run_arq_property_test.sh --num-runs=50
tools/run_card_fat_property_test.sh --num-runs=50

echo "[gate] mutation checks"
tools/mutations/mtx_identity_zero_diag.sh
tools/mutations/dvd_fast_open_force_fail.sh
tools/mutations/arq_drop_hi_callback.sh
tools/mutations/card_alloc_no_freeblock_decrement.sh

echo "[gate] PASS"
