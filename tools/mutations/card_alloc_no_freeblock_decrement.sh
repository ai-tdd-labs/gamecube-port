#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$repo_root"

tools/run_mutation_check.sh tools/mutations/card_alloc_no_freeblock_decrement.patch -- \
  tools/run_card_fat_property_test.sh --num-runs=20

