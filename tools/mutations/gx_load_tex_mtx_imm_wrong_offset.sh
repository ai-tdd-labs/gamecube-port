#!/usr/bin/env bash
set -euo pipefail

# Mutation check for GXLoadTexMtxImm: wrong post-transform base (0x400 instead of 0x500)
#
# Usage:
#   tools/mutations/gx_load_tex_mtx_imm_wrong_offset.sh <test_case_dir> [<test_case_dir2> ...]

repo_root="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$repo_root"

if [[ $# -lt 1 ]]; then
  echo "usage: $0 <test_case_dir> [<test_case_dir2> ...]" >&2
  exit 2
fi

args=("tools/run_mutation_check.sh" "tools/mutations/gx_load_tex_mtx_imm_wrong_offset.patch" "--")

first=1
for d in "$@"; do
  if [[ $first -eq 0 ]]; then
    args+=("::")
  fi
  first=0
  args+=("tools/run_host_scenario.sh" "$d")
done

"${args[@]}"
