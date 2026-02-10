#!/usr/bin/env bash
set -euo pipefail

# Mutation check for PADControlMotor: force command to 0 and ensure replay cases FAIL.
#
# Usage:
#   tools/mutations/pad_control_motor_zero_cmd.sh <trace_case_dir> [<trace_case_dir2> ...]

repo_root="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$repo_root"

if [[ $# -lt 1 ]]; then
  echo "usage: $0 <trace_case_dir> [<trace_case_dir2> ...]" >&2
  exit 2
fi

args=("tools/run_mutation_check.sh" "tools/mutations/pad_control_motor_zero_cmd.patch" "--")

first=1
for d in "$@"; do
  if [[ $first -eq 0 ]]; then
    args+=("::")
  fi
  first=0
  args+=("tools/replay_trace_case_pad_control_motor.sh" "$d")
done

"${args[@]}"

