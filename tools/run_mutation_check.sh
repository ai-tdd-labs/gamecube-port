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

source "$repo_root/tools/helpers/lock.sh"

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

# Safety: refuse to run if the patch is already applied.
# If the reverse patch applies cleanly, it means the patch is currently active.
if git apply --reverse --check "$patch_file" >/dev/null 2>&1; then
  echo "fatal: mutation patch already applied: $patch_file" >&2
  echo "hint: run 'git apply -R $patch_file' to revert it, or reset your worktree" >&2
  exit 2
fi

cleanup() {
  # Best-effort revert; don't fail cleanup.
  git apply -R "$patch_file" >/dev/null 2>&1 || true
  release_lock
}
trap cleanup EXIT INT TERM HUP

acquire_lock "gc-trace-replay" 600
export GC_LOCK_HELD=1

# Apply mutant patch.
if ! git apply "$patch_file" >/dev/null 2>&1; then
  echo "fatal: failed to apply mutation patch: $patch_file" >&2
  echo "hint: patch likely out-of-date with current code; regenerate or update the patch." >&2
  exit 2
fi

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
  # Replay scripts normally refuse to run on a dirty worktree. During mutation
  # checks we intentionally dirty the tree (apply a patch), so allow a bypass.
  #
  # Only scenario runners need the extra oracle compare. Trace-replay scripts
  # already encode a predicate and should not enable GC_SCENARIO_COMPARE, because
  # their underlying scenarios often use dynamic out paths.
  if [[ "${c[0]}" == *"/run_host_scenario.sh" ]]; then
    GC_ALLOW_DIRTY=1 GC_SCENARIO_COMPARE=1 "${c[@]}"
  else
    GC_ALLOW_DIRTY=1 GC_SCENARIO_COMPARE=0 "${c[@]}"
  fi
  rc=$?
  set -e
  if [[ $rc -eq 0 ]]; then
    echo "fatal: mutation survived (command exited 0): ${c[*]}" >&2
    exit 1
  fi
  if [[ $rc -eq 2 ]]; then
    echo "fatal: mutation check infra error (exit 2): ${c[*]}" >&2
    echo "hint: exit 2 usually means missing fixtures/usage error, not a killed mutant." >&2
    exit 2
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
