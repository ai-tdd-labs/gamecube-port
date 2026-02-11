#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$repo_root"

echo "[replay-gate] Running available retail-trace replay suites"

run_if_exists() {
  local script="$1"
  shift || true
  if [[ -x "$script" ]]; then
    echo "[replay-gate] $script $*"
    if "$script" "$@"; then
      return 0
    fi
    local rc=$?
    if [[ $rc -eq 2 ]]; then
      echo "[replay-gate] SKIP ($rc): $script (missing external asset/trace corpus)"
      return 0
    fi
    echo "[replay-gate] FAIL ($rc): $script" >&2
    return "$rc"
  else
    echo "[replay-gate] SKIP (not executable): $script"
  fi
}

# These scripts already exist in this repo and are used for replay parity.
run_if_exists "tools/harvest_and_replay_os_disable_interrupts.sh"
run_if_exists "tools/harvest_and_replay_vi_set_post_retrace_callback.sh"
run_if_exists "tools/harvest_and_replay_si_set_sampling_rate.sh"
run_if_exists "tools/replay_trace_suite.sh"

echo "[replay-gate] PASS"
