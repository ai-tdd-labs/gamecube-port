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
  echo "fatal: RVZ path not found. Usage: $0 /path/to/Mario\\ Party\\ 4\\ \\(USA\\).rvz" >&2
  exit 2
fi

echo "[hupadinit-blockers] rvz=$rvz"
echo "[hupadinit-blockers] GC_MAX_UNIQUE=${GC_MAX_UNIQUE:-10} GC_MAX_HITS=${GC_MAX_HITS:-200} GC_TIMEOUT=${GC_TIMEOUT:-150}"

echo "[1/4] OSDisableInterrupts / OSRestoreInterrupts"
"$repo_root/tools/harvest_and_replay_os_disable_interrupts.sh" "$rvz"

echo "[2/4] VISetPostRetraceCallback"
"$repo_root/tools/harvest_and_replay_vi_set_post_retrace_callback.sh" "$rvz"

echo "[3/4] SISetSamplingRate"
"$repo_root/tools/harvest_and_replay_si_set_sampling_rate.sh"

echo "[4/4] PADControlMotor"
"$repo_root/tools/harvest_and_replay_pad_control_motor.sh" "$rvz"

echo "[ok] all HuPadInit blocker harvest/replay steps completed"
