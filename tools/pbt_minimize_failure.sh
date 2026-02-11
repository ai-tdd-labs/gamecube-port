#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage:
  tools/pbt_minimize_failure.sh --cmd "<runner command>" --seed <N> [--steps <N>] [--max-tries <N>]

Purpose:
  Best-effort minimizer for deterministic PBT/parity failures.
  It reruns the same command with nearby seeds and fewer steps to find a smaller repro.

Example:
  tools/pbt_minimize_failure.sh \
    --cmd "bash tools/run_osthread_property_test.sh --op=L11 -v" \
    --seed 123 --steps 2000
EOF
}

cmd=""
seed=""
steps=""
max_tries=40

while [[ $# -gt 0 ]]; do
  case "$1" in
    --cmd) cmd="$2"; shift 2 ;;
    --seed) seed="$2"; shift 2 ;;
    --steps) steps="$2"; shift 2 ;;
    --max-tries) max_tries="$2"; shift 2 ;;
    -h|--help) usage; exit 0 ;;
    *) echo "unknown arg: $1" >&2; usage; exit 2 ;;
  esac
done

if [[ -z "$cmd" || -z "$seed" ]]; then
  usage
  exit 2
fi

base_cmd="$cmd --seed=$seed"
if [[ -n "$steps" ]]; then
  base_cmd="$base_cmd --steps=$steps"
fi

echo "[min] baseline: $base_cmd"
if eval "$base_cmd" >/dev/null 2>&1; then
  echo "[min] baseline PASS; nothing to minimize"
  exit 0
fi

best_seed="$seed"
best_steps="${steps:-0}"
echo "[min] baseline FAIL (seed=$best_seed steps=${best_steps:-n/a})"

# Try nearby seeds first.
for ((i=1; i<=max_tries; i++)); do
  cand=$((seed - i))
  if ((cand < 0)); then
    break
  fi
  test_cmd="$cmd --seed=$cand"
  if [[ -n "$steps" ]]; then
    test_cmd="$test_cmd --steps=$steps"
  fi
  if ! eval "$test_cmd" >/dev/null 2>&1; then
    best_seed="$cand"
    echo "[min] improved seed: $best_seed"
  fi
done

# If steps provided, try halving to reduce repro size.
if [[ -n "$steps" ]]; then
  curr="$steps"
  while ((curr > 1)); do
    cand=$((curr / 2))
    test_cmd="$cmd --seed=$best_seed --steps=$cand"
    if ! eval "$test_cmd" >/dev/null 2>&1; then
      curr="$cand"
      best_steps="$curr"
      echo "[min] reduced steps: $best_steps"
    else
      break
    fi
  done
fi

echo
echo "[min] minimal repro candidate:"
if [[ -n "$steps" ]]; then
  echo "$cmd --seed=$best_seed --steps=$best_steps -v"
else
  echo "$cmd --seed=$best_seed -v"
fi
