#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$repo_root"

tools/run_mutation_check.sh \
  tools/mutations/gx_set_tex_coord_scale_manually_no_minus1.patch \
  -- \
  tools/run_gx_set_tex_coord_scale_manually_pbt.sh
