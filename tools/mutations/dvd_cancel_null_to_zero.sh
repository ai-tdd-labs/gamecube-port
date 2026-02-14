#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$repo_root"

tools/run_mutation_check.sh tools/mutations/dvd_cancel_null_to_zero.patch -- \
  tools/run_host_scenario.sh tests/sdk/dvd/dvd_cancel/host/dvd_cancel_generic_001_scenario.c
