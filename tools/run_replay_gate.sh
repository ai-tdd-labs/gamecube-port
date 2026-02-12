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
    local rc=0
    set +e
    "$script" "$@"
    rc=$?
    set -e
    if [[ $rc -eq 0 ]]; then
      return 0
    fi
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

# Fast local replay parity over existing harvested traces.
run_if_exists "tools/replay_trace_suite.sh"

# Optional refresh from Dolphin/RVZ (disabled by default because it needs
# external assets and is slower).
if [[ "${GC_REPLAY_HARVEST:-0}" == "1" ]]; then
  run_if_exists "tools/harvest_and_replay_os_disable_interrupts.sh"
  run_if_exists "tools/harvest_and_replay_vi_set_post_retrace_callback.sh"
  run_if_exists "tools/harvest_and_replay_si_set_sampling_rate.sh"
fi

echo "[replay-gate] PASS"
