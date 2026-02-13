#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$repo_root"

tools/run_mutation_check.sh \
  tools/mutations/ar_get_dma_status_const0.patch \
  -- \
  tools/run_ar_get_dma_status_pbt.sh
