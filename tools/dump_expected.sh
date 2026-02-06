#!/usr/bin/env bash
set -euo pipefail

# Dump expected RAM region from running a DOL in Dolphin via GDB stub.
# Usage:
#   tools/dump_expected.sh <dol_path> <out_bin> [addr] [size] [run_seconds] [chunk]

DOL_PATH=${1:?dol_path required}
OUT_BIN=${2:?out_bin required}
ADDR=${3:-0x80300000}
SIZE=${4:-0x40}
RUN=${5:-0.5}
CHUNK=${6:-}

repo_root="$(cd "$(dirname "$0")/.." && pwd)"

if [[ "$DOL_PATH" != /* ]]; then
  DOL_PATH="$repo_root/$DOL_PATH"
fi

if [[ "$OUT_BIN" != /* ]]; then
  OUT_BIN="$repo_root/$OUT_BIN"
fi

mkdir -p "$(dirname "$OUT_BIN")"

# Keep the workflow deterministic: kill stale headless Dolphin instances that
# might still be bound to the GDB stub port (e.g. after a crash).
pkill -f "Dolphin -b -d -e" >/dev/null 2>&1 || true

# Occasional transient failures can happen while Dolphin is starting up.
# Retry a few times to keep the workflow deterministic and low-friction.
for attempt in 1 2 3; do
  if PYTHONUNBUFFERED=1 python3 -u tools/ram_dump.py \
      --exec "$DOL_PATH" \
      --addr "$ADDR" \
      --size "$SIZE" \
      --out "$OUT_BIN" \
      --run "$RUN" \
      --halt \
      ${CHUNK:+--chunk "$CHUNK"}
  then
    exit 0
  fi
  echo "[dump_expected] attempt $attempt failed; retrying..." >&2
  sleep 1
done

echo "[dump_expected] failed after retries" >&2
exit 1
