#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$repo_root"

tools/run_mutation_check.sh \
  tools/mutations/gx_init_tlut_obj_bad_shift.patch \
  -- \
  tools/run_gx_init_tlut_obj_pbt.sh
