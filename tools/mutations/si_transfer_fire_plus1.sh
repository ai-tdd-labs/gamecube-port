#!/usr/bin/env bash
set -euo pipefail

# Mutation check for SITransfer: make packet->fire off-by-one and ensure
# at least one replay case FAILs.
#
# Usage:
#   tools/mutations/si_transfer_fire_plus1.sh <trace_case_dir> [<trace_case_dir2> ...]

repo_root="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$repo_root"

if [[ $# -lt 1 ]]; then
  echo "usage: $0 <trace_case_dir> [<trace_case_dir2> ...]" >&2
  exit 2
fi

args=("tools/run_mutation_check.sh" "tools/mutations/si_transfer_fire_plus1.patch" "--")

# Expect each replay command to FAIL under the mutant.
first=1
for d in "$@"; do
  if [[ $first -eq 0 ]]; then
    args+=("::")
  fi
  first=0
  args+=("tools/replay_trace_case_si_transfer.sh" "$d")
done

"${args[@]}"
