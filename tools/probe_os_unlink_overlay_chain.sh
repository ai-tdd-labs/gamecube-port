#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"

rvz="${1:-}"
if [[ -z "$rvz" ]]; then
  rvz="$(python3 - <<'PY'
from pathlib import Path
p = Path("docs/sdk/mp4/MP4_assets.md")
if p.exists():
    for line in p.read_text().splitlines():
        if line.strip().startswith("- Path:"):
            val = line.split(":", 1)[1].strip()
            print(val.replace(chr(96), ""))
            break
PY
)"
fi

if [[ -z "$rvz" || ! -f "$rvz" ]]; then
  echo "fatal: RVZ path not found. Usage: $0 /path/to/Mario\\ Party\\ 4\\ (USA).rvz [timeout_s]" >&2
  exit 2
fi

timeout_s="${2:-180}"
out_root="${3:-$repo_root/tests/trace-harvest/os_unlink/probes}"
stamp="$(date +%Y%m%d_%H%M%S)"
run_dir="$out_root/$stamp"
mkdir -p "$run_dir"

# RVZ cold boot can take longer before GDB stub is reachable.
# Empirically 12s avoids intermittent connection-refused starts on macOS.
export DOLPHIN_START_DELAY="${DOLPHIN_START_DELAY:-12}"

declare -a names=("omOvlGotoEx" "omOvlKill" "omDLLNumEnd" "OSUnlink")
declare -a pcs=("0x8002EEC0" "0x8002F014" "0x80031FA0" "0x800B8180")

echo "[info] RVZ: $rvz"
echo "[info] timeout: ${timeout_s}s"
echo "[info] dolphin_start_delay: ${DOLPHIN_START_DELAY}s"
echo "[info] out: $run_dir"

for i in "${!names[@]}"; do
  name="${names[$i]}"
  pc="${pcs[$i]}"
  out_bin="$run_dir/${name}.bin"
  log_txt="$run_dir/${name}.log"

  echo "[probe] $name @ $pc"
  if bash "$repo_root/tools/dump_expected_rvz_probe_at_pc.sh" \
      "$rvz" "$pc" "$out_bin" "$timeout_s" 1 >"$log_txt" 2>&1; then
    if [[ -s "$out_bin" ]]; then
      echo "[hit] $name ($pc)"
    else
      echo "[miss] $name ($pc) (no dump produced)"
    fi
  else
    echo "[miss] $name ($pc) (probe command failed; see $log_txt)"
  fi
done

echo "[done] probe results in: $run_dir"
