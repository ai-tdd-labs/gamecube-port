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

rvz="${1:-$default_rvz}"
out_dir="${2:-$repo_root/tests/trace-harvest/card_probe_ex/mp4_rvz_boot_v1}"
timeout_s="${3:-${GC_TIMEOUT:-120}}"

if [[ -z "$rvz" || ! -f "$rvz" ]]; then
  echo "fatal: rvz not found: $rvz" >&2
  exit 2
fi

userdir="$repo_root/tests/trace-harvest/card_probe_ex/dolphin_user_boot_v1"

rm -rf "$userdir"
mkdir -p "$userdir/Config" "$out_dir"

# Ensure GDB stub uses the expected port when running under an isolated userdir.
cat >"$userdir/Config/Dolphin.ini" <<EOF
[General]
GDBPort = 9090
[Interface]
ShowDebuggingUI = True
DebugModeEnabled = True
ConfirmStop = False
EOF

export DOLPHIN_START_DELAY="${DOLPHIN_START_DELAY:-12}"

cleanup() {
  pkill -f "Dolphin -b -d" >/dev/null 2>&1 || true
}
trap cleanup EXIT

# CARDProbeEx entry PC in MP4 (USA): 0x800D4840 (per decomp symbol address).
# Dumps:
# - in/out memSize and sectorSize (stack pointers from r4/r5)
# - GameChoice region (fixed address in SDK: 0x800030E3)
# - function prologue bytes (helps recover global addresses like __CARDBlock)
python3 "$repo_root/tools/trace_pc_entry_exit.py" \
  --rvz "$rvz" \
  --dolphin-userdir "$userdir" \
  --entry-pc 0x800D4840 \
  --out-dir "$out_dir" \
  --max-unique "${GC_MAX_UNIQUE:-10}" \
  --max-hits "${GC_MAX_HITS:-200}" \
  --timeout "$timeout_s" \
  --delay "${DOLPHIN_START_DELAY}" \
  --enable-mmu \
  --dump mem_size:@r4:0x4 \
  --dump sector_size:@r5:0x4 \
  --dump gamechoice:0x800030E0:0x20 \
  --dump code:0x800D4840:0x80

echo "[ok] harvested CARDProbeEx cases under: $out_dir"

