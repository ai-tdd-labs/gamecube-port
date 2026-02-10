#!/usr/bin/env bash
set -euo pipefail

# Run all mutation scripts and summarize results.
#
# Categories:
# - KILLED: the mutation script exited 0 (all commands failed under mutant) -> good
# - SURVIVED: the mutation script exited 1 -> bad (tests did not detect the bug)
# - INFRA: the mutation script exited 2 -> setup/fixture error (missing expected, usage, etc.)
# - TIMEOUT: script exceeded timeout -> investigate (hang or very slow)
#
# Output:
# - docs/codex/mutations/report_<timestamp>.csv
# - docs/codex/mutations/report_latest.csv (copy)
#
# Environment:
# - GC_MUT_TIMEOUT_S: per-script timeout (default 30s). Uses gtimeout/timeout if available.
# - GC_MAIN_REPO_ROOT: a checkout that contains local-only fixtures (expected/ and tests/traces).

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$repo_root"

timeout_s=${GC_MUT_TIMEOUT_S:-30}

# Auto-detect the main repo root if we are in a .worktrees/* checkout.
if [[ -z "${GC_MAIN_REPO_ROOT:-}" ]]; then
  if [[ "$repo_root" == */.worktrees/* ]]; then
    GC_MAIN_REPO_ROOT="$(cd "$repo_root/../.." && pwd)"
  else
    GC_MAIN_REPO_ROOT="$repo_root"
  fi
fi
export GC_MAIN_REPO_ROOT

out_dir="$repo_root/docs/codex/mutations"
mkdir -p "$out_dir"

ts="$(date +%Y%m%d_%H%M%S)"
report="$out_dir/report_${ts}.csv"
latest="$out_dir/report_latest.csv"

echo "script,status,rc,seconds,arg,note" >"$report"

total=0
killed=0
survived=0
infra=0
timed=0

pick_suite_arg() {
  local base="$1"
  local cmdline="$2"

  # If this mutant replays a retail trace, pick the first available trace case.
  if echo "$cmdline" | rg -q "replay_trace_case_"; then
    local suite
    suite="$(echo "$base" | sed -E 's/_(always_one|skip_.*|swap_.*|wrong_.*|return_.*|off_by_one|zero|negate|decrement|corrupt|flip_.*|no_.*)$//')"
    local trace_root="$GC_MAIN_REPO_ROOT/tests/traces/$suite"
    if [[ -d "$trace_root" ]]; then
      local hit
      hit="$(find "$trace_root" -type d -name 'hit_*' 2>/dev/null | sort | head -n 1 || true)"
      if [[ -n "${hit:-}" ]]; then
        echo "$hit"
        return 0
      fi
    fi
    echo ""
    return 0
  fi

  # Scenario-based: choose the first host scenario for the suite.
  if echo "$cmdline" | rg -q "run_host_scenario\\.sh"; then
    local suite
    suite="$(echo "$base" | sed -E 's/_(always_one|skip_.*|swap_.*|wrong_.*|return_.*|off_by_one|zero|negate|decrement|corrupt|flip_.*|no_.*)$//')"
    local scen
    scen="$(find "$repo_root/tests/sdk" -type f -path "*/${suite}/host/*_scenario.c" 2>/dev/null | sort | head -n 1 || true)"
    if [[ -n "${scen:-}" ]]; then
      echo "$scen"
      return 0
    fi
    echo ""
    return 0
  fi

  echo ""
}

run_one() {
  local script=$1
  local base cmdline arg start end rc status note secs
  base="$(basename "$script" .sh)"
  cmdline="$(sed -n '1,120p' "$script" 2>/dev/null || true)"
  arg="$(pick_suite_arg "$base" "$cmdline")"

  start="$(date +%s)"
  set +e
  if command -v gtimeout >/dev/null 2>&1; then
    if [[ -n "${arg:-}" ]]; then
      gtimeout "${timeout_s}" bash "$script" "$arg" >/dev/null 2>&1
    else
      gtimeout "${timeout_s}" bash "$script" >/dev/null 2>&1
    fi
    rc=$?
  elif command -v timeout >/dev/null 2>&1; then
    if [[ -n "${arg:-}" ]]; then
      timeout "${timeout_s}" bash "$script" "$arg" >/dev/null 2>&1
    else
      timeout "${timeout_s}" bash "$script" >/dev/null 2>&1
    fi
    rc=$?
  else
    # macOS default: no timeout.
    if [[ -n "${arg:-}" ]]; then
      bash "$script" "$arg" >/dev/null 2>&1
    else
      bash "$script" >/dev/null 2>&1
    fi
    rc=$?
  fi
  set -e
  end="$(date +%s)"
  secs=$((end - start))

  note=""
  if [[ $rc -eq 124 ]]; then
    status="TIMEOUT"
    timed=$((timed + 1))
    note="timeout_s=${timeout_s}"
  elif [[ $rc -eq 0 ]]; then
    status="KILLED"
    killed=$((killed + 1))
  elif [[ $rc -eq 1 ]]; then
    status="SURVIVED"
    survived=$((survived + 1))
  elif [[ $rc -eq 2 ]]; then
    status="INFRA"
    infra=$((infra + 1))
  else
    status="INFRA"
    infra=$((infra + 1))
    note="unexpected_rc"
  fi

  # CSV: escape commas in note/arg by replacing with semicolons.
  note="${note//,/;}"
  arg="${arg//,/;}"
  echo "$(basename "$script"),$status,$rc,$secs,$arg,$note" >>"$report"
}

while IFS= read -r script; do
  total=$((total + 1))
  run_one "$script"
done < <(find "$repo_root/tools/mutations" -maxdepth 1 -type f -name "*.sh" | sort)

cp -f "$report" "$latest"

echo "[mut-report] wrote: $report" >&2
echo "[mut-report] wrote: $latest" >&2
echo "[mut-report] total=$total killed=$killed survived=$survived infra=$infra timeout=$timed" >&2

# Non-zero if any survived; suitable for gating.
if [[ $survived -ne 0 ]]; then
  exit 1
fi

