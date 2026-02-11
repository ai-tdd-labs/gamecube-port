#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$repo_root"

tools/run_mutation_check.sh tools/mutations/mtx_identity_zero_diag.patch -- \
  tools/run_pbt.sh mtx_core 5000 0xC0DEC0DE

