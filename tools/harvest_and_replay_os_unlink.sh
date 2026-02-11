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

out_dir="${2:-$repo_root/tests/traces/os_unlink/mp4_rvz_v1}"
mkdir -p "$out_dir"

python3 "$repo_root/tools/trace_pc_entry_exit.py" \
  --rvz "$rvz" \
  --entry-pc 0x800B8180 \
  --out-dir "$out_dir" \
  --max-unique "${GC_MAX_UNIQUE:-10}" \
  --max-hits "${GC_MAX_HITS:-300}" \
  --timeout "${GC_TIMEOUT:-180}" \
  --enable-mmu \
  --dump module_info:@r3:0x100 \
  --dump os_module_list:0x800030C8:0x20 \
  --dump os_string_table:0x800030D0:0x10

replay="$repo_root/tools/replay_trace_case_os_unlink.sh"
if [[ -x "$replay" ]]; then
  echo "[replay] running host replays..."
  for d in "$out_dir"/hit_*; do
    [[ -d "$d" ]] || continue
    GC_ALLOW_DIRTY=1 "$replay" "$d"
  done
else
  echo "[note] replay script not present yet: $replay"
fi

echo "[ok] harvested OSUnlink cases under: $out_dir"
