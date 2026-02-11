#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$repo_root"

tools/run_mutation_check.sh tools/mutations/arq_drop_hi_callback.patch -- \
  tools/run_arq_property_test.sh --num-runs=20

