#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$repo_root"

tools/run_mutation_check.sh \
  tools/mutations/gx_tex_coord_2s16_drop_sign.patch \
  -- \
  tools/run_gx_tex_coord_2s16_pbt.sh
