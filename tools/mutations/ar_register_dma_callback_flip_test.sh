#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$repo_root"

tools/run_mutation_check.sh \
  tools/mutations/ar_register_dma_callback_no_store.patch \
  -- \
  tools/run_ar_register_dma_callback_pbt.sh
