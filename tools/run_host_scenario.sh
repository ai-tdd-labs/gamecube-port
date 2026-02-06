#!/usr/bin/env bash
set -euo pipefail

# Build+run one host scenario to produce actual.bin.
#
# Usage:
#   tools/run_host_scenario.sh <tests/sdk/.../host/<scenario>.c>
#
# The scenario provides:
#   gc_scenario_label()
#   gc_scenario_out_path()
#   gc_scenario_run(GcRam*)
#
# The runner dumps 0x80300000..0x80300040 to gc_scenario_out_path().

SCENARIO_SRC=${1:?scenario .c path required}

repo_root="$(cd "$(dirname "$0")/.." && pwd)"

if [[ "$SCENARIO_SRC" != /* ]]; then
  SCENARIO_SRC="$repo_root/$SCENARIO_SRC"
fi

scenario_dir="$(cd "$(dirname "$SCENARIO_SRC")" && pwd)"
scenario_base="$(basename "$SCENARIO_SRC" .c)"
exe="$scenario_dir/${scenario_base}_host"

# Infer subsystem from the path: .../tests/sdk/<subsystem>/...
subsystem="$(echo "$SCENARIO_SRC" | sed -n 's|.*tests/sdk/\([^/]*\)/.*|\1|p')"
if [[ -z "$subsystem" ]]; then
  echo "fatal: could not infer subsystem from path: $SCENARIO_SRC" >&2
  exit 2
fi

port_srcs=()
case "$subsystem" in
  os)
    port_srcs+=(
      "$repo_root/src/sdk_port/os/OSArena.c"
      "$repo_root/src/sdk_port/os/OSCache.c"
      "$repo_root/src/sdk_port/os/OSAlloc.c"
      "$repo_root/src/sdk_port/os/OSInit.c"
      "$repo_root/src/sdk_port/os/OSFastCast.c"
      "$repo_root/src/sdk_port/os/OSError.c"
      "$repo_root/src/sdk_port/os/OSSystem.c"
      "$repo_root/src/sdk_port/os/OSRtc.c"
    )
    ;;
  vi)
    port_srcs+=(
      "$repo_root/src/sdk_port/vi/VI.c"
    )
    ;;
  pad)
    port_srcs+=(
      "$repo_root/src/sdk_port/pad/PAD.c"
    )
    ;;
  gx)
    port_srcs+=(
      "$repo_root/src/sdk_port/gx/GX.c"
    )
    ;;
  dvd)
    port_srcs+=(
      "$repo_root/src/sdk_port/dvd/DVD.c"
    )
    ;;
  *)
    echo "fatal: no sdk_port sources configured for subsystem: $subsystem" >&2
    exit 2
    ;;
esac

echo "[host-build] $SCENARIO_SRC"
cc -O2 -g \
  -I"$repo_root/tests" \
  -I"$repo_root/tests/harness" \
  -I"$repo_root/src" \
  -I"$repo_root/src/sdk_port" \
  "$repo_root/tests/harness/gc_host_ram.c" \
  "$repo_root/tests/harness/gc_host_runner.c" \
  "$repo_root/src/sdk_port/gc_mem.c" \
  "${port_srcs[@]}" \
  "$SCENARIO_SRC" \
  -o "$exe"

echo "[host-run] $exe"
(cd "$scenario_dir" && "$exe")
