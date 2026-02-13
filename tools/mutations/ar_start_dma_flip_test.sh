#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$repo_root"

tools/run_mutation_check.sh \
  tools/mutations/ar_start_dma_swap_mmem_aram.patch \
  -- \
  tools/run_ar_start_dma_pbt.sh
