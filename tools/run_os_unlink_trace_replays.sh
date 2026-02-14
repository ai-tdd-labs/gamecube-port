#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"

cases_dir="${1:-$repo_root/tests/trace-harvest/os_unlink/mp4_rvz_pipe_v1}"
if [[ "$cases_dir" != /* ]]; then
  cases_dir="$repo_root/$cases_dir"
fi
if [[ ! -d "$cases_dir" ]]; then
  echo "fatal: cases dir not found: $cases_dir" >&2
  exit 2
fi

replay="$repo_root/tools/replay_trace_case_os_unlink.sh"
if [[ ! -x "$replay" ]]; then
  echo "fatal: replay script not found/executable: $replay" >&2
  exit 2
fi

count=0
for d in "$cases_dir"/hit_*; do
  [[ -d "$d" ]] || continue
  count=$((count + 1))
  echo "[case] $(basename "$d")"
  GC_ALLOW_DIRTY="${GC_ALLOW_DIRTY:-0}" "$replay" "$d" >/dev/null
done

if [[ "$count" -eq 0 ]]; then
  echo "fatal: no hit_* case dirs found under: $cases_dir" >&2
  exit 2
fi

echo "PASS: OSUnlink trace replays ($count cases)"

