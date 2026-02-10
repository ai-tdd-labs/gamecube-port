#!/usr/bin/env bash
set -euo pipefail

# Mutation check for OSDisableInterrupts: always return 1.
#
# Usage:
#   tools/mutations/os_disable_interrupts_always_one.sh <trace_case_dir> [<trace_case_dir2> ...]

repo_root="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$repo_root"

if [[ $# -lt 1 ]]; then
  echo "usage: $0 <trace_case_dir> [<trace_case_dir2> ...]" >&2
  exit 2
fi

args=("tools/run_mutation_check.sh" "tools/mutations/os_disable_interrupts_always_one.patch" "--")

first=1
for d in "$@"; do
  if [[ $first -eq 0 ]]; then
    args+=("::")
  fi
  first=0
  args+=("tools/replay_trace_case_os_disable_interrupts.sh" "$d")
done

"${args[@]}"

