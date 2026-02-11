#!/usr/bin/env bash
set -euo pipefail

# Mutation check for GXSetDispCopyYScale: wrong scale mask (0x1FE instead of 0x1FF)
#
# Usage:
#   tools/mutations/gx_set_disp_copy_y_scale_wrong_mask.sh <test_case_dir> [<test_case_dir2> ...]

repo_root="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$repo_root"

if [[ $# -lt 1 ]]; then
  echo "usage: $0 <test_case_dir> [<test_case_dir2> ...]" >&2
  exit 2
fi

args=("tools/run_mutation_check.sh" "tools/mutations/gx_set_disp_copy_y_scale_wrong_mask.patch" "--")

first=1
for d in "$@"; do
  if [[ $first -eq 0 ]]; then
    args+=("::")
  fi
  first=0
  args+=("tools/run_host_scenario.sh" "$d")
done

"${args[@]}"
