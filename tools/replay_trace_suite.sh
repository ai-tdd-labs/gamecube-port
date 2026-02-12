#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$repo_root"

# Run available trace-replay case scripts over local trace corpora.
# Defaults to a fast smoke pass (1 case per corpus). Override with:
#   GC_REPLAY_MAX_CASES=<n>
#   GC_REPLAY_CASE_GLOB='tests/traces/.../hit_*'

max_cases="${GC_REPLAY_MAX_CASES:-1}"
case_glob="${GC_REPLAY_CASE_GLOB:-}"

run_one() {
  local script="$1"
  local case_dir="$2"
  if [[ ! -x "$script" ]]; then
    echo "[replay-suite] SKIP script not executable: $script"
    return 0
  fi
  echo "[replay-suite] $script $(basename "$case_dir")"
  local rc=0
  set +e
  GC_ALLOW_DIRTY=1 "$script" "$case_dir"
  rc=$?
  set -e
  if [[ "$rc" -eq 0 ]]; then
    return 0
  fi
  if [[ "$rc" -eq 2 ]]; then
    echo "[replay-suite] SKIP case not runnable: $case_dir"
    return 0
  fi
  echo "[replay-suite] FAIL ($rc): $script $case_dir" >&2
  return "$rc"
}

run_glob() {
  local script="$1"
  local glob="$2"
  local count=0
  local found=0
  for d in $glob; do
    [[ -d "$d" ]] || continue
    found=1
    run_one "$script" "$d"
    count=$((count + 1))
    if [[ "$max_cases" -gt 0 && "$count" -ge "$max_cases" ]]; then
      break
    fi
  done
  if [[ "$found" -eq 0 ]]; then
    echo "[replay-suite] SKIP no cases for $script ($glob)"
  fi
}

if [[ -n "$case_glob" ]]; then
  run_glob "$repo_root/tools/replay_trace_case_os_disable_interrupts.sh" "$case_glob"
  exit 0
fi

# OS
run_glob "$repo_root/tools/replay_trace_case_os_disable_interrupts.sh" "tests/traces/os_disable_interrupts/mp4_rvz_v1/hit_*"
run_glob "$repo_root/tools/replay_trace_case_os_link.sh" "tests/traces/os_link/mp4_rvz_v1/hit_*"
run_glob "$repo_root/tools/replay_trace_case_os_unlink.sh" "tests/traces/os_unlink/synth_case_*"

# VI
run_glob "$repo_root/tools/replay_trace_case_vi_set_post_retrace_callback.sh" "tests/traces/vi_set_post_retrace_callback/mp4_rvz_postcb/hit_*"

# SI
run_glob "$repo_root/tools/replay_trace_case_si_set_sampling_rate.sh" "tests/traces/si_set_sampling_rate/mp4_rvz_si_ctrl/hit_*"
run_glob "$repo_root/tools/replay_trace_case_si_get_response.sh" "tests/traces/si_get_response/mp4_rvz_v1/hit_*"
run_glob "$repo_root/tools/replay_trace_case_si_transfer.sh" "tests/traces/si_transfer/mp4_rvz_v4/hit_*"

# PAD
run_glob "$repo_root/tools/replay_trace_case_pad_set_spec.sh" "tests/traces/pad_set_spec/mp4_rvz_v1/hit_*"
run_glob "$repo_root/tools/replay_trace_case_pad_init.sh" "tests/traces/pad_init/mp4_rvz_v1/hit_*"
run_glob "$repo_root/tools/replay_trace_case_pad_read.sh" "tests/traces/pad_read/mp4_rvz/hit_*"
run_glob "$repo_root/tools/replay_trace_case_pad_reset.sh" "tests/traces/pad_reset/mp4_rvz_v2/hit_*"
run_glob "$repo_root/tools/replay_trace_case_pad_clamp.sh" "tests/traces/pad_clamp/mp4_rvz/hit_*"
run_glob "$repo_root/tools/replay_trace_case_pad_control_motor.sh" "tests/traces/pad_control_motor/mp4_rvz_v1/hit_*"

echo "[replay-suite] PASS"
