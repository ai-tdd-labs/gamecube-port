#!/usr/bin/env bash
set -euo pipefail

# Windows variant of dump_expected.sh
#
# Key differences from macOS version:
#   - Uses Windows Dolphin path from dolphin_win.conf
#   - Converts MSYS2 paths to Windows paths for Dolphin.exe
#   - Uses taskkill instead of pkill
#   - Uses python (not python3) if python3 is missing
#
# Usage:
#   tools/dump_expected_win.sh <dol_path> <out_bin> [addr] [size] [run_seconds] [chunk] [halt_0_or_1]

DOL_PATH=${1:?dol_path required}
OUT_BIN=${2:?out_bin required}
ADDR=${3:-0x80300000}
SIZE=${4:-0x40}
RUN=${5:-0.5}
CHUNK=${6:-}
HALT=${7:-1}
WAIT_ADDR=${8:-}
WAIT_VALUE=${9:-}
WAIT_TIMEOUT=${10:-}

repo_root="$(cd "$(dirname "$0")/.." && pwd)"

# Load Windows Dolphin config
DOLPHIN_BIN="C:\\projects\\dolphin-gdb\\Binary\\x64\\Dolphin.exe"
if [[ -f "$repo_root/tools/dolphin_win.conf" ]]; then
    source "$repo_root/tools/dolphin_win.conf"
fi

# Resolve relative paths to absolute
if [[ "$DOL_PATH" != /* ]]; then
    DOL_PATH="$repo_root/$DOL_PATH"
fi
if [[ "$OUT_BIN" != /* ]]; then
    OUT_BIN="$repo_root/$OUT_BIN"
fi

mkdir -p "$(dirname "$OUT_BIN")"

# Convert MSYS2 path to Windows path for Dolphin.exe
# /d/foo/bar -> D:\foo\bar
msys_to_win() {
    local p="$1"
    if [[ "$p" =~ ^/([a-zA-Z])/ ]]; then
        local drive="${BASH_REMATCH[1]^^}"
        p="${drive}:${p:2}"
    fi
    echo "${p//\//\\}"
}

DOL_PATH_WIN="$(msys_to_win "$DOL_PATH")"

# Kill stale Dolphin instances (Windows: taskkill)
taskkill //F //IM Dolphin.exe 2>/dev/null || true
sleep 0.5

# Find Python
PYTHON=""
for try in python3 python; do
    if command -v "$try" >/dev/null 2>&1; then PYTHON="$try"; break; fi
done
if [[ -z "$PYTHON" ]]; then
    # Try common Windows Python locations
    for try in "/c/Python312/python.exe" "/c/Python311/python.exe" \
               "/c/Users/$USER/AppData/Local/Programs/Python/Python312/python.exe" \
               "/c/Users/$USER/AppData/Local/Programs/Python/Python311/python.exe"; do
        if [[ -x "$try" ]]; then PYTHON="$try"; break; fi
    done
fi
if [[ -z "$PYTHON" ]]; then
    echo "ERROR: no Python found. Install Python 3 and ensure it's on PATH."
    exit 2
fi

echo "[dump-expected-win] DOLPHIN=$DOLPHIN_BIN"
echo "[dump-expected-win] PYTHON=$PYTHON"
echo "[dump-expected-win] DOL=$DOL_PATH_WIN"

for attempt in 1 2 3; do
    if PYTHONUNBUFFERED=1 "$PYTHON" -u "$repo_root/tools/ram_dump.py" \
        --dolphin "$DOLPHIN_BIN" \
        --exec "$DOL_PATH_WIN" \
        --enable-mmu \
        --addr "$ADDR" \
        --size "$SIZE" \
        --out "$OUT_BIN" \
        --run "$RUN" \
        ${HALT:+$([[ "$HALT" == "1" ]] && echo --halt || true)} \
        ${WAIT_ADDR:+--wait-u32be-addr "$WAIT_ADDR"} \
        ${WAIT_VALUE:+--wait-u32be-value "$WAIT_VALUE"} \
        ${WAIT_TIMEOUT:+--wait-timeout "$WAIT_TIMEOUT"} \
        ${CHUNK:+--chunk "$CHUNK"}
    then
        exit 0
    fi
    echo "[dump-expected-win] attempt $attempt failed; retrying..." >&2
    taskkill //F //IM Dolphin.exe 2>/dev/null || true
    sleep 1
done

echo "[dump-expected-win] failed after retries" >&2
exit 1
