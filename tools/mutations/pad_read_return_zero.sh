#!/usr/bin/env bash
set -euo pipefail

# Mutation check for PADRead: return 0 instead of PAD_CHAN0_BIT.
#
# Usage:
#   tools/mutations/pad_read_return_zero.sh <trace_case_dir> [<trace_case_dir2> ...]

repo_root="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$repo_root"

if [[ $# -lt 1 ]]; then
  echo "usage: $0 <trace_case_dir> [<trace_case_dir2> ...]" >&2
  exit 2
fi

args=("tools/run_mutation_check.sh" "tools/mutations/pad_read_return_zero.patch" "--")

first=1
for d in "$@"; do
  if [[ $first -eq 0 ]]; then
    args+=("::")
  fi
  first=0
  args+=("tools/replay_trace_case_pad_read.sh" "$d")
done

"${args[@]}"

