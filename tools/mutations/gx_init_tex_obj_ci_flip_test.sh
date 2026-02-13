#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$repo_root"

tools/run_mutation_check.sh \
  tools/mutations/gx_init_tex_obj_ci_keep_ci_flag.patch \
  -- \
  tools/run_gx_init_tex_obj_ci_pbt.sh
