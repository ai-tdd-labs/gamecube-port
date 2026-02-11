#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"

# SISetSamplingRate retail trace harvesting needs a dump window that includes the
# SI control register snapshot used by the replay harness (out_si_ctrl.bin).
# We keep the harvested traces local-only (gitignored) and focus this script on
# replaying the existing trace corpus deterministically.

trace_dir="${1:-$repo_root/tests/traces/si_set_sampling_rate/mp4_rvz_si_ctrl}"

if [[ "$trace_dir" != /* ]]; then
  trace_dir="$repo_root/$trace_dir"
fi

trace_jsonl="$trace_dir/trace.jsonl"
if [[ ! -f "$trace_jsonl" ]]; then
  echo "fatal: trace.jsonl not found: $trace_jsonl" >&2
  echo "hint: harvest traces first into $trace_dir" >&2
  exit 2
fi

max_unique="${GC_MAX_UNIQUE:-10}"

case_ids="$(python3 - "$trace_jsonl" "$max_unique" <<'PY'
import json,sys
p=sys.argv[1]
max_u=int(sys.argv[2])
seen=set(); out=[]
for line in open(p,'r'):
  line=line.strip()
  if not line:
    continue
  o=json.loads(line)
  k=o.get('dedupe_key')
  if not k or k in seen:
    continue
  seen.add(k)
  cid=o.get('case_id')
  if cid:
    out.append(cid)
  if len(out)>=max_u:
    break
for cid in out:
  print(cid)
PY
)"

if [[ -z "${case_ids//[[:space:]]/}" ]]; then
  echo "fatal: no unique cases found in $trace_jsonl" >&2
  exit 2
fi

uniq_count="$(printf "%s\n" "$case_ids" | sed '/^$/d' | wc -l | tr -d ' ')"
echo "[replay] SISetSamplingRate unique cases=$uniq_count (trace_dir=$(basename "$trace_dir"))"

while IFS= read -r cid; do
  [[ -n "$cid" ]] || continue
  d="$trace_dir/$cid"
  echo "== $cid"
  GC_ALLOW_DIRTY=1 "$repo_root/tools/replay_trace_case_si_set_sampling_rate.sh" "$d"
done <<<"$case_ids"

echo "[ok] replayed SISetSamplingRate unique cases from: $trace_dir"
