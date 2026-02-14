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
out_dir="${2:-$repo_root/tests/trace-harvest/os_unlink/mp4_rvz_pipe_v1}"
timeout_s="${3:-${GC_TIMEOUT:-300}}"

if [[ -z "$rvz" || ! -f "$rvz" ]]; then
  echo "fatal: rvz not found: $rvz" >&2
  exit 2
fi

userdir="$repo_root/tests/trace-harvest/os_unlink/dolphin_user_pipe_v1"
pipe_name="mp4_overlay_pipe_001"
fifo="$userdir/Pipes/$pipe_name"

rm -rf "$userdir"
mkdir -p "$userdir/Config" "$userdir/Pipes" "$out_dir"

mkfifo "$fifo"

# Ensure GDB stub uses the expected port when running under an isolated userdir.
cat >"$userdir/Config/Dolphin.ini" <<EOF
[General]
GDBPort = 9090
[Interface]
ShowDebuggingUI = True
DebugModeEnabled = True
ConfirmStop = False
EOF

# Minimal per-run controller mapping:
# - Use Dolphin "Pipe" backend device for GCPad1.
# - Map a few buttons we can script to navigate MP4 menus.
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
  --entry-pc 0x800B8180 \
  --out-dir "$out_dir" \
  --max-unique "${GC_MAX_UNIQUE:-10}" \
  --max-hits "${GC_MAX_HITS:-300}" \
  --timeout "$timeout_s" \
  --delay 12 \
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
fi

echo "[ok] harvested OSUnlink (pipe) cases under: $out_dir"
