#!/usr/bin/env bash
set -euo pipefail

# Run a mutation check:
# - Apply a patch that introduces a bug (the mutant)
# - Run one or more commands that are expected to FAIL
# - Revert the patch (always)
#
# Usage:
#   tools/run_mutation_check.sh <patch_file> -- <command> [args...] [:: <command2> ...]
#
# Example:
#   tools/run_mutation_check.sh tools/mutations/si_transfer_fire_plus1.patch -- \
#     tools/replay_trace_case_si_transfer.sh tests/traces/si_transfer/mp4_rvz_v4/hit_000002_pc_800D9CC4_lr_800DA26C

patch_file=${1:?patch_file required}
shift

if [[ ${1:-} != "--" ]]; then
  echo "fatal: expected -- after patch_file" >&2
  exit 2
fi
shift

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$repo_root"

if [[ ! -f "$patch_file" ]]; then
  echo "fatal: patch not found: $patch_file" >&2
  exit 2
fi

# Safety: refuse to run if the worktree is dirty.
if ! git diff --quiet || ! git diff --cached --quiet; then
  echo "fatal: git worktree has uncommitted changes; commit/stash first" >&2
  git status --porcelain=v1 >&2 || true
  exit 2
fi

cleanup() {
  # Best-effort revert; don't fail cleanup.
  git apply -R "$patch_file" >/dev/null 2>&1 || true
}
trap cleanup EXIT

# Apply mutant patch.
git apply "$patch_file"

# Run commands (split by "::" separator). Each command must fail.
any_ran=0
cmd=()
run_cmd_expect_fail() {
  local -a c=("${cmd[@]}")
  if [[ ${#c[@]} -eq 0 ]]; then
    return
  fi
  any_ran=1
  echo "[mut] expect FAIL: ${c[*]}" >&2
  set +e
  "${c[@]}"
  rc=$?
  set -e
  if [[ $rc -eq 0 ]]; then
    echo "fatal: mutation survived (command exited 0): ${c[*]}" >&2
    exit 1
  fi
  echo "[mut] ok (failed as expected): rc=$rc" >&2
  cmd=()
}

for arg in "$@"; do
  if [[ "$arg" == "::" ]]; then
    run_cmd_expect_fail
    continue
  fi
  cmd+=("$arg")
done
run_cmd_expect_fail

if [[ $any_ran -eq 0 ]]; then
  echo "fatal: no commands provided" >&2
  exit 2
fi

echo "[mut] PASS: all commands failed under mutant" >&2
