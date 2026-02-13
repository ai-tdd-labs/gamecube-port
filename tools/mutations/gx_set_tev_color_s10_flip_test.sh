#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$repo_root"

tools/run_mutation_check.sh \
  tools/mutations/gx_set_tev_color_s10_wrong_g_shift.patch \
  -- \
  tools/run_gx_set_tev_color_s10_pbt.sh
