#!/usr/bin/env bash
set -euo pipefail

# Dump expected RAM region from running a DOL in Dolphin via GDB stub.
# Usage:
#   tools/dump_expected.sh <dol_path> <out_bin> [addr] [size] [run_seconds] [chunk] [halt_0_or_1] [wait_u32be_addr] [wait_u32be_value] [wait_timeout]

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

if [[ "$DOL_PATH" != /* ]]; then
  DOL_PATH="$repo_root/$DOL_PATH"
fi

if [[ "$OUT_BIN" != /* ]]; then
  OUT_BIN="$repo_root/$OUT_BIN"
fi

mkdir -p "$(dirname "$OUT_BIN")"

source "$repo_root/tools/helpers/lock.sh"

# Prevent parallel runs from interfering with each other (Dolphin + GDB stub
# port are singletons). Without this, multiple dump_expected runs can kill each
# other's Dolphin instances and make suites flaky.
acquire_lock "gc-trace-replay" 600
trap release_lock EXIT INT TERM HUP

# Keep the workflow deterministic: if a stale headless Dolphin is still bound to
# the GDB stub port (9090), terminate only that listener.
#
# This is intentionally narrower than the old global pkill, so unrelated Dolphin
# instances are not affected.
if command -v lsof >/dev/null 2>&1; then
  stale_pid="$(
    (lsof -nP -iTCP:9090 -sTCP:LISTEN 2>/dev/null || true) | awk '
      NR==1 { next } # header
      $1 ~ /Dolphin/ { print $2; exit 0 }
    '
  )"
  if [[ -n "${stale_pid:-}" ]]; then
    kill -TERM "$stale_pid" >/dev/null 2>&1 || true
    sleep 0.2
    kill -KILL "$stale_pid" >/dev/null 2>&1 || true
  fi
fi

# Optional: isolate Dolphin user/config for determinism (memcard/SRAM/etc).
#
# Environment variables:
# - DOLPHIN_USERDIR: passed to ram_dump.py --dolphin-userdir
# - DOLPHIN_CONFIG: semicolon-separated list of Dolphin -C overrides, e.g.
#     DOLPHIN_CONFIG="Core.MMU=True;Core.MemcardAPath=/tmp/CardA.raw"
userdir_arg=()
if [[ -n "${DOLPHIN_USERDIR:-}" ]]; then
  userdir_arg=(--dolphin-userdir "$DOLPHIN_USERDIR")

  # Ensure the stub listens on the expected port under an isolated userdir.
  # Dolphin reads this from the userdir's Config/Dolphin.ini.
  cfg_dir="$DOLPHIN_USERDIR/Config"
  cfg_ini="$cfg_dir/Dolphin.ini"
  mkdir -p "$cfg_dir"
  if [[ ! -f "$cfg_ini" ]]; then
    cat >"$cfg_ini" <<EOF
[General]
GDBPort = 9090
[Interface]
ShowDebuggingUI = True
DebugModeEnabled = True
ConfirmStop = False
EOF
  fi
fi

config_args=()
if [[ -n "${DOLPHIN_CONFIG:-}" ]]; then
  # Split on ';' into multiple --dolphin-config args.
  IFS=';' read -r -a __cfgs <<<"$DOLPHIN_CONFIG"
  for c in "${__cfgs[@]}"; do
    c="${c#"${c%%[![:space:]]*}"}" # ltrim
    c="${c%"${c##*[![:space:]]}"}" # rtrim
    [[ -n "$c" ]] || continue
    config_args+=(--dolphin-config "$c")
  done
fi

# Occasional transient failures can happen while Dolphin is starting up.
# Retry a few times to keep the workflow deterministic and low-friction.
for attempt in 1 2 3; do
  if PYTHONUNBUFFERED=1 python3 -u tools/ram_dump.py \
      --exec "$DOL_PATH" \
      --enable-mmu \
      "${userdir_arg[@]+${userdir_arg[@]}}" \
      "${config_args[@]+${config_args[@]}}" \
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
  echo "[dump_expected] attempt $attempt failed; retrying..." >&2
  sleep 1
done

echo "[dump_expected] failed after retries" >&2
exit 1
