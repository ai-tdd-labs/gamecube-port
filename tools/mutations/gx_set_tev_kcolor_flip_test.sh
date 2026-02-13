#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$repo_root"

tools/run_mutation_check.sh \
  tools/mutations/gx_set_tev_kalpha_sel_shift.patch \
  -- \
  tools/run_gx_set_tev_kcolor_pbt.sh
