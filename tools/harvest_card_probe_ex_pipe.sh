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
out_dir="${2:-$repo_root/tests/trace-harvest/card_probe_ex/mp4_rvz_pipe_v1}"
timeout_s="${3:-${GC_TIMEOUT:-300}}"

if [[ -z "$rvz" || ! -f "$rvz" ]]; then
  echo "fatal: rvz not found: $rvz" >&2
  exit 2
fi

userdir="$repo_root/tests/trace-harvest/card_probe_ex/dolphin_user_pipe_v1"
pipe_name="mp4_card_pipe_001"
fifo="$userdir/Pipes/$pipe_name"

rm -rf "$userdir"
mkdir -p "$userdir/Config" "$userdir/Pipes" "$out_dir"

mkfifo "$fifo"

cat >"$userdir/Config/Dolphin.ini" <<EOF
[General]
GDBPort = 9090
[Interface]
ShowDebuggingUI = True
DebugModeEnabled = True
ConfirmStop = False
EOF

cat >"$userdir/Config/GCPadNew.ini" <<EOF
[GCPad1]
Device = Pipe/0/$pipe_name
Buttons/A = \`Button A\`
Buttons/B = \`Button B\`
Buttons/Start = \`Button START\`
D-Pad/Up = \`Button D_UP\`
D-Pad/Down = \`Button D_DOWN\`
D-Pad/Left = \`Button D_LEFT\`
D-Pad/Right = \`Button D_RIGHT\`
[GCPad2]
Device =
[GCPad3]
Device =
[GCPad4]
Device =
EOF

export DOLPHIN_START_DELAY="${DOLPHIN_START_DELAY:-12}"

writer_pid=""
cleanup() {
  if [[ -n "$writer_pid" ]]; then
    kill "$writer_pid" >/dev/null 2>&1 || true
    wait "$writer_pid" >/dev/null 2>&1 || true
  fi
  pkill -f "Dolphin -b -d" >/dev/null 2>&1 || true
}
trap cleanup EXIT

python3 "$repo_root/tools/helpers/dolphin_pipe_writer.py" \
  --fifo "$fifo" \
  --boot-wait 12 \
  --duration "$timeout_s" \
  --period 0.55 \
  >/dev/null 2>&1 &
writer_pid="$!"

python3 "$repo_root/tools/trace_pc_entry_exit.py" \
  --rvz "$rvz" \
  --dolphin-userdir "$userdir" \
  --entry-pc 0x800D4840 \
  --out-dir "$out_dir" \
  --max-unique "${GC_MAX_UNIQUE:-10}" \
  --max-hits "${GC_MAX_HITS:-500}" \
  --timeout "$timeout_s" \
  --delay "${DOLPHIN_START_DELAY}" \
  --enable-mmu \
  --dump mem_size:@r4:0x4 \
  --dump sector_size:@r5:0x4 \
  --dump gamechoice:0x800030E0:0x20 \
  --dump code:0x800D4840:0x80

echo "[ok] harvested CARDProbeEx (pipe) cases under: $out_dir"

