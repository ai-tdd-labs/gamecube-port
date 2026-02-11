#!/usr/bin/env bash
set -euo pipefail

# Mutation check for DCInvalidateRange: swap addr and len fields.
#
# Usage:
#   tools/mutations/os_dc_invalidate_range_swap_fields.sh <test_case_dir> [<test_case_dir2> ...]

repo_root="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$repo_root"

if [[ $# -lt 1 ]]; then
  echo "usage: $0 <test_case_dir> [<test_case_dir2> ...]" >&2
  exit 2
fi

args=("tools/run_mutation_check.sh" "tools/mutations/os_dc_invalidate_range_swap_fields.patch" "--")

first=1
for d in "$@"; do
  if [[ $first -eq 0 ]]; then args+=("::"); fi
  first=0
  args+=("tools/run_host_scenario.sh" "$d")
done

"${args[@]}"
