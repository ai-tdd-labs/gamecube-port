#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$repo_root"

tools/run_mutation_check.sh \
  tools/mutations/gx_set_tev_ind_tile_wrong_wrap.patch \
  -- \
  tools/run_gx_set_tev_ind_tile_pbt.sh
