#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$repo_root"

tools/run_mutation_check.sh \
  tools/mutations/gx_normal_1x16_truncate.patch \
  -- \
  tools/run_gx_normal_1x16_pbt.sh
