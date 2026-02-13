#!/usr/bin/env bash
set -euo pipefail

# Mutation check for VISetBlack: invert the boolean test (0u/1u swapped).
# Runs against the unified L0-L5 DOL-PBT suite.

repo_root="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$repo_root"

tools/run_mutation_check.sh \
  tools/mutations/vi_set_black_flip_test.patch \
  -- \
  tools/run_vi_set_black_pbt.sh
