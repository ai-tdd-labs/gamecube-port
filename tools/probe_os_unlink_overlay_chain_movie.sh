#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"

default_rvz="$(python3 - <<'PY'
from pathlib import Path
p = Path("docs/sdk/mp4/MP4_assets.md")
if p.exists():
    for line in p.read_text().splitlines():
        if line.strip().startswith("- Path:"):
            print(line.split(":", 1)[1].strip().replace(chr(96), ""))
            break
PY
)"
default_dtm="$repo_root/tests/trace-harvest/os_unlink/inputs/mp4_overlay_trigger_001.dtm"

rvz="${1:-$default_rvz}"
timeout_s="${2:-180}"
out_root="${3:-$repo_root/tests/trace-harvest/os_unlink/probes}"
dtm="${4:-$default_dtm}"

if [[ -z "$rvz" || ! -f "$rvz" ]]; then
  echo "fatal: rvz not found: $rvz" >&2
  exit 2
fi
if [[ ! -f "$dtm" ]]; then
  echo "fatal: dtm not found: $dtm" >&2
  echo "record one per docs/sdk/mp4/Dolphin_input_automation.md and retry" >&2
  exit 2
fi

export DOLPHIN_MOVIE="$dtm"

exec bash "$repo_root/tools/probe_os_unlink_overlay_chain.sh" "$rvz" "$timeout_s" "$out_root"

