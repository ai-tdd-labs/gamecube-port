#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$repo_root"

tools/run_mutation_check.sh tools/mutations/dvd_fast_open_force_fail.patch -- \
  tools/run_pbt.sh dvd_core 3000 0xC0DEC0DE

