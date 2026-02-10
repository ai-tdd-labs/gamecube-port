#!/usr/bin/env bash
set -euo pipefail

# Mutation check for GXAdjustForOverscan: change hor*2 to hor*3
#
# Usage:
#   tools/mutations/gx_adjust_for_overscan_hor_times3.sh <test_case_dir> [<test_case_dir2> ...]

repo_root="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$repo_root"

if [[ $# -lt 1 ]]; then
  echo "usage: $0 <test_case_dir> [<test_case_dir2> ...]" >&2
  exit 2
fi

args=("tools/run_mutation_check.sh" "tools/mutations/gx_adjust_for_overscan_hor_times3.patch" "--")

first=1
for d in "$@"; do
  if [[ $first -eq 0 ]]; then
    args+=("::")
  fi
  first=0
  args+=("tools/run_host_scenario.sh" "$d")
done

"${args[@]}"
