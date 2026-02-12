#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"

rvz="${1:-}"
if [[ -z "$rvz" ]]; then
  rvz="$(python3 - <<'PY'
from pathlib import Path
p=Path('docs/sdk/mp4/MP4_assets.md')
if p.exists():
  for line in p.read_text().splitlines():
    if line.strip().startswith('- Path:'):
      print(line.split(':',1)[1].strip().strip('`'))
      break
PY
)"
fi

if [[ -z "$rvz" || ! -f "$rvz" ]]; then
  echo "fatal: RVZ path not found. Usage: $0 /path/to/Mario\ Party\ 4\ (USA).rvz" >&2
  exit 2
fi

out_dir="$repo_root/tests/trace-harvest/os_disable_interrupts/mp4_rvz_v1"
mkdir -p "$out_dir"

python3 "$repo_root/tools/trace_pc_entry_exit.py" \
  --rvz "$rvz" \
  --entry-pc 0x800B723C \
  --out-dir "$out_dir" \
  --max-unique "${GC_MAX_UNIQUE:-10}" \
  --max-hits "${GC_MAX_HITS:-200}" \
  --timeout "${GC_TIMEOUT:-120}" \
  --enable-mmu

echo "[replay] running host replays..."
for d in "$out_dir"/hit_*; do
  [[ -d "$d" ]] || continue
  GC_ALLOW_DIRTY=1 "$repo_root/tools/replay_trace_case_os_disable_interrupts.sh" "$d"
done

echo "[ok] harvested + replayed OSDisableInterrupts cases under: $out_dir"
