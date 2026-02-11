#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"

rvz="${1:-}"
dtm="${2:-}"
if [[ -z "$rvz" || -z "$dtm" ]]; then
  echo "usage: $0 /path/to/Mario\\ Party\\ 4\\ \\(USA\\).rvz /path/to/overlay_trigger.dtm [out_dir]" >&2
  exit 2
fi

if [[ ! -f "$rvz" ]]; then
  echo "fatal: rvz not found: $rvz" >&2
  exit 2
fi
if [[ ! -f "$dtm" ]]; then
  echo "fatal: dtm not found: $dtm" >&2
  exit 2
fi

out_dir="${3:-$repo_root/tests/traces/os_unlink/mp4_rvz_movie_v1}"
mkdir -p "$out_dir"

python3 "$repo_root/tools/trace_pc_entry_exit.py" \
  --rvz "$rvz" \
  --movie "$dtm" \
  --entry-pc 0x800B8180 \
  --out-dir "$out_dir" \
  --max-unique "${GC_MAX_UNIQUE:-10}" \
  --max-hits "${GC_MAX_HITS:-300}" \
  --timeout "${GC_TIMEOUT:-300}" \
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
