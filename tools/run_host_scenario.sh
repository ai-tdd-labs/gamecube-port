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

# Keep repo clean: build host executables into an ignored temp build dir,
# not next to the scenario source.
build_dir="$repo_root/tests/build/host_scenarios"
mkdir -p "$build_dir"
exe="$build_dir/${scenario_base}_host_$$"

# Infer subsystem from the path: .../tests/sdk/<subsystem>/...
subsystem="$(echo "$SCENARIO_SRC" | sed -n 's|.*tests/sdk/\([^/]*\)/.*|\1|p')"
if [[ -z "$subsystem" ]]; then
  # Workloads live outside tests/sdk.
  if echo "$SCENARIO_SRC" | rg -q "/tests/workload/"; then
    subsystem="workload"
  else
    echo "fatal: could not infer subsystem from path: $SCENARIO_SRC" >&2
    exit 2
  fi
fi

# Smoke suites may span multiple subsystems; compile the union.
if [[ "$subsystem" == "smoke" ]]; then
  subsystem="os+dvd+vi+pad+gx"
fi

extra_includes=()
extra_srcs=()
extra_cflags=()
case "$subsystem" in
  workload)
    # Allow building small integration workloads against a minimal, host-safe
    # header surface that matches the subset of the SDK used by the workload.
    extra_includes+=(
      -I"$repo_root/tests/workload/include"
      -I"$repo_root/src/game_workload/mp4/vendor/src"
    )
    # For now, we only support the HuSysInit workload.
    extra_srcs+=(
      "$repo_root/src/game_workload/mp4/vendor/src/game/init.c"
      "$repo_root/src/game_workload/mp4/vendor/src/game/pad.c"
      "$repo_root/src/game_workload/mp4/vendor/src/game/process.c"
      "$repo_root/tests/workload/mp4/slices/gwinit_only.c"
      "$repo_root/tests/workload/mp4/slices/pfinit_only.c"
      "$repo_root/tests/workload/mp4/slices/husprinit_only.c"
      "$repo_root/tests/workload/mp4/slices/huperfinit_only.c"
      "$repo_root/tests/workload/mp4/slices/hu3dinit_only.c"
      "$repo_root/tests/workload/mp4/slices/hu3d_preproc_only.c"
      "$repo_root/tests/workload/mp4/slices/hudatainit_only.c"
      "$repo_root/tests/workload/mp4/slices/hudvderrorwatch_only.c"
      "$repo_root/tests/workload/mp4/slices/husoftresetbuttoncheck_only.c"
      "$repo_root/tests/workload/mp4/slices/msmmusfdoutend_only.c"
    )

    # Only link heavy-module stubs for scenarios that explicitly need them.
    case "$scenario_base" in
      mp4_init_to_viwait_001_scenario|mp4_mainloop_one_iter_001_scenario|mp4_mainloop_two_iter_001_scenario)
        extra_srcs+=("$repo_root/tests/workload/mp4/slices/post_sprinit_stubs.c")
        ;;
    esac
    # Make the workload deterministic and avoid pulling in decomp build-system macros.
    extra_cflags+=(
      -DVERSION=0
      -DVERSION_PAL=0
      -DVERSION_NTSC=1
    )
    subsystem="os+dvd+vi+pad+gx"
    ;;
esac

port_srcs=()
case "$subsystem" in
  os|os+dvd+vi+pad+gx)
    port_srcs+=(
      "$repo_root/src/sdk_port/os/OSArena.c"
      "$repo_root/src/sdk_port/os/OSCache.c"
      "$repo_root/src/sdk_port/os/OSAlloc.c"
      "$repo_root/src/sdk_port/os/OSInterrupts.c"
      "$repo_root/src/sdk_port/os/OSInit.c"
      "$repo_root/src/sdk_port/os/OSFastCast.c"
      "$repo_root/src/sdk_port/os/OSError.c"
      "$repo_root/src/sdk_port/os/OSSystem.c"
      "$repo_root/src/sdk_port/os/OSRtc.c"
      "$repo_root/src/sdk_port/os/OSStopwatch.c"
    )
    ;;
esac

case "$subsystem" in
  si|os+dvd+vi+pad+gx)
    port_srcs+=(
      "$repo_root/src/sdk_port/os/OSInterrupts.c"
      "$repo_root/src/sdk_port/os/OSError.c"
      "$repo_root/src/sdk_port/vi/VI.c"
      "$repo_root/src/sdk_port/si/SI.c"
    )
    ;;
esac

case "$subsystem" in
  vi|os+dvd+vi+pad+gx)
    port_srcs+=(
      "$repo_root/src/sdk_port/os/OSInterrupts.c"
      "$repo_root/src/sdk_port/vi/VI.c"
    )
    ;;
esac

case "$subsystem" in
  pad|os+dvd+vi+pad+gx)
    port_srcs+=(
      "$repo_root/src/sdk_port/os/OSInterrupts.c"
      "$repo_root/src/sdk_port/os/OSError.c"
      "$repo_root/src/sdk_port/vi/VI.c"
      "$repo_root/src/sdk_port/si/SI.c"
      "$repo_root/src/sdk_port/pad/PAD.c"
    )
    ;;
esac

case "$subsystem" in
  gx|os+dvd+vi+pad+gx)
    port_srcs+=(
      "$repo_root/src/sdk_port/gx/GX.c"
    )
    ;;
esac

case "$subsystem" in
  dvd|os+dvd+vi+pad+gx)
    port_srcs+=(
      "$repo_root/src/sdk_port/dvd/DVD.c"
    )
    ;;
esac

# De-duplicate sources (some union subsystems add the same file via multiple cases).
# macOS ships Bash 3.2 by default (no associative arrays), so keep this portable.
if [[ ${#port_srcs[@]} -gt 0 ]]; then
  __uniq=()
  for s in "${port_srcs[@]}"; do
    __skip=0
    for u in "${__uniq[@]+${__uniq[@]}}"; do
      if [[ "$u" == "$s" ]]; then
        __skip=1
        break
      fi
    done
    if [[ $__skip -eq 0 ]]; then
      __uniq+=("$s")
    fi
  done
  port_srcs=("${__uniq[@]+${__uniq[@]}}")
fi

if [[ ${#port_srcs[@]} -eq 0 ]]; then
  echo "fatal: no sdk_port sources configured for subsystem: $subsystem" >&2
  exit 2
fi

echo "[host-build] $SCENARIO_SRC"
ld_gc_flags=()
case "$(uname -s)" in
  Darwin) ld_gc_flags+=(-Wl,-dead_strip) ;;
  *) ld_gc_flags+=(-Wl,--gc-sections) ;;
esac

cc -O2 -g0 -ffunction-sections -fdata-sections \
  "${extra_cflags[@]+${extra_cflags[@]}}" \
  -I"$repo_root/tests" \
  -I"$repo_root/tests/harness" \
  -I"$repo_root/src" \
  -I"$repo_root/src/sdk_port" \
  "${extra_includes[@]+${extra_includes[@]}}" \
  "$repo_root/tests/harness/gc_host_ram.c" \
  "$repo_root/tests/harness/gc_host_runner.c" \
  "$repo_root/src/sdk_port/gc_mem.c" \
  "${port_srcs[@]}" \
  "${extra_srcs[@]+${extra_srcs[@]}}" \
  "$SCENARIO_SRC" \
  "${ld_gc_flags[@]}" \
  -o "$exe"

echo "[host-run] $exe"
(cd "$scenario_dir" && "$exe")

rm -f "$exe" >/dev/null 2>&1 || true
