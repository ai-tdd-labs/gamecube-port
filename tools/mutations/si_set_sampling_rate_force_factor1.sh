#!/usr/bin/env bash
set -euo pipefail

# Mutation check for SISetSamplingRate: force factor=1 (ignore VI54 bit0).
#
# Usage:
#   tools/mutations/si_set_sampling_rate_force_factor1.sh <trace_case_dir> [<trace_case_dir2> ...]

repo_root="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$repo_root"

if [[ $# -lt 1 ]]; then
  echo "usage: $0 <trace_case_dir> [<trace_case_dir2> ...]" >&2
  exit 2
fi

args=("tools/run_mutation_check.sh" "tools/mutations/si_set_sampling_rate_force_factor1.patch" "--")

first=1
for d in "$@"; do
  if [[ $first -eq 0 ]]; then
    args+=("::")
  fi
  first=0
  args+=("tools/replay_trace_case_si_set_sampling_rate.sh" "$d")
done

"${args[@]}"

