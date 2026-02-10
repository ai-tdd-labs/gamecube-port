#!/usr/bin/env bash
set -euo pipefail

# Mutation check for GXInitTexObj: wrong bit position for wrap_s (2 instead of 0)
#
# Usage:
#   tools/mutations/gx_init_tex_obj_wrong_wrap_shift.sh <test_case_dir> [<test_case_dir2> ...]

repo_root="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$repo_root"

if [[ $# -lt 1 ]]; then
  echo "usage: $0 <test_case_dir> [<test_case_dir2> ...]" >&2
  exit 2
fi

args=("tools/run_mutation_check.sh" "tools/mutations/gx_init_tex_obj_wrong_wrap_shift.patch" "--")

first=1
for d in "$@"; do
  if [[ $first -eq 0 ]]; then
    args+=("::")
  fi
  first=0
  args+=("tools/run_host_scenario.sh" "$d")
done

"${args[@]}"
