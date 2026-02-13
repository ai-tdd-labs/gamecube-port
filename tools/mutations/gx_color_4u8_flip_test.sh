#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$repo_root"

tools/run_mutation_check.sh \
  tools/mutations/gx_color_4u8_swap_rg.patch \
  -- \
  tools/run_gx_color_4u8_pbt.sh
