#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$repo_root"

tools/run_mutation_check.sh \
  tools/mutations/gx_load_tlut_wrong_offset_mask.patch \
  -- \
  tools/run_gx_load_tlut_pbt.sh
