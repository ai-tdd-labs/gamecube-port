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

is_workload=0

# Smoke suites may span multiple subsystems; compile the union.
if [[ "$subsystem" == "smoke" ]]; then
  subsystem="os+dvd+vi+pad+gx"
fi

extra_includes=()
extra_srcs=()
extra_cflags=()
case "$subsystem" in
  workload)
    is_workload=1
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
      "$repo_root/tests/workload/mp4/slices/memory_heap_host.c"
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

    # Coroutine/context-switch implementation for HuPrc* on host.
    #
    # Options:
    # - ucontext (default): portable and "good enough" for our deterministic host workloads
    # - asm: aarch64 save/restore experiment (currently unstable)
    GC_HOST_JMP_IMPL=${GC_HOST_JMP_IMPL:-ucontext}
    case "$GC_HOST_JMP_IMPL" in
      ucontext)
        extra_srcs+=("$repo_root/tests/workload/mp4/slices/jmp_ucontext.c")
        ;;
      asm)
        extra_srcs+=(
          "$repo_root/tests/workload/mp4/slices/jmp_host.c"
          "$repo_root/tests/workload/mp4/slices/jmp_aarch64.S"
        )
        ;;
      *)
        echo "fatal: unknown GC_HOST_JMP_IMPL=$GC_HOST_JMP_IMPL (expected ucontext|asm)" >&2
        exit 2
        ;;
    esac

    # Only link heavy-module stubs for scenarios that explicitly need them.
    case "$scenario_base" in
      mp4_init_to_viwait_001_scenario|mp4_mainloop_one_iter_001_scenario|mp4_mainloop_one_iter_tick_001_scenario|mp4_mainloop_two_iter_001_scenario|mp4_mainloop_two_iter_tick_001_scenario)
        extra_srcs+=("$repo_root/tests/workload/mp4/slices/post_sprinit_stubs.c")
        ;;
    esac
    # mp4_init_to_viwait uses WipeInit as a reachability stub.
    case "$scenario_base" in
      mp4_init_to_viwait_001_scenario)
        extra_srcs+=("$repo_root/tests/workload/mp4/slices/wipeinit_stub.c")
        ;;
    esac
    # mp4_mainloop_* scenarios call WipeExecAlways(). Keep it out of the scenario TU
    # so we can later swap in a decomp slice without editing the scenario. For now,
    # we link a minimal decomp slice that does not emulate GX drawing.
    case "$scenario_base" in
      mp4_mainloop_one_iter_001_scenario|mp4_mainloop_one_iter_tick_001_scenario|mp4_mainloop_two_iter_001_scenario|mp4_mainloop_two_iter_tick_001_scenario)
        extra_srcs+=("$repo_root/tests/workload/mp4/slices/wipeexecalways_decomp_blank.c")
        ;;
    esac
    # pfDrawFonts() is game-specific and GX-heavy; keep it as a host-safe slice.
    case "$scenario_base" in
      mp4_mainloop_one_iter_001_scenario|mp4_mainloop_one_iter_tick_001_scenario|mp4_mainloop_two_iter_001_scenario|mp4_mainloop_two_iter_tick_001_scenario)
        extra_srcs+=("$repo_root/tests/workload/mp4/slices/pfdrawfonts_stub.c")
        ;;
    esac
    # Make the workload deterministic and avoid pulling in decomp build-system macros.
    extra_cflags+=(
      -DVERSION=0
      -DVERSION_PAL=0
      -DVERSION_NTSC=1
      -DGC_HOST_WORKLOAD=1
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
      "$repo_root/src/sdk_port/os/OSModule.c"
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
  mtx)
    port_srcs+=(
      "$repo_root/src/sdk_port/mtx/mtx.c"
      "$repo_root/src/sdk_port/mtx/mtx44.c"
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

case "$subsystem" in
  ar)
    port_srcs+=(
      "$repo_root/src/sdk_port/ar/ar_hw.c"
    )
    ;;
esac

case "$subsystem" in
  ai)
    port_srcs+=(
      "$repo_root/src/sdk_port/ai/ai.c"
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

# Debug mode for host scenarios (keeps behavior identical, just changes debugability).
opt_flags=(-O2 -g0)
if [[ "${GC_HOST_DEBUG:-0}" == "1" ]]; then
  opt_flags=(-O0 -g)
fi

cc "${opt_flags[@]}" -ffunction-sections -fdata-sections \
  -D_XOPEN_SOURCE=700 \
  "${extra_cflags[@]+${extra_cflags[@]}}" \
  -I"$repo_root/tests" \
  -I"$repo_root/tests/harness" \
  -I"$repo_root/tests/workload/include" \
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
run_env=()
if [[ "$is_workload" -eq 1 ]]; then
  # Workloads use the mp4 decomp callsites and benefit from a retail-like BootInfo arenaHi.
  # Keep this conservative: only set arenaHi by default; arenaLo can remain default unless needed.
  if [[ -z "${GC_HOST_BOOT_ARENA_HI:-}" ]]; then
    run_env+=("GC_HOST_BOOT_ARENA_HI=0x817FDEA0")
  fi
fi
if [[ -n "${GC_JMP_DEBUG:-}" ]]; then
  run_env+=("GC_JMP_DEBUG=$GC_JMP_DEBUG")
fi
if [[ -n "${GC_PRC_DEBUG:-}" ]]; then
  run_env+=("GC_PRC_DEBUG=$GC_PRC_DEBUG")
fi

(cd "$scenario_dir" && \
  if [[ "${GC_HOST_LLDB:-0}" == "1" ]]; then
    lldb --batch \
      -o "settings set target.env-vars ${run_env[*]-}" \
      -o run \
      -o "register read pc lr sp x0 x1 x18 x19 x20" \
      -o bt \
      -- "$exe"
  else
    env "${run_env[@]+${run_env[@]}}" "$exe"
  fi)

# Optional oracle compare step (used by mutation tests).
# Many host scenarios only *produce* actual/ output. For mutation testing we need
# a predicate that fails when output changes. We derive the output/expected paths
# from the scenario source.
if [[ "${GC_SCENARIO_COMPARE:-0}" == "1" ]]; then
  # Extract the literal string returned by gc_scenario_out_path(). Keep this
  # robust across formatting by scanning forward for the first `return "..."`.
  out_rel="$(
    awk '
      /gc_scenario_out_path[[:space:]]*[(]/ { in_fn=1 }
      in_fn && /return[[:space:]]*"/ {
        match($0, /return[[:space:]]*"[^"]+"/)
        if (RSTART > 0) {
          s=substr($0, RSTART, RLENGTH)
          sub(/^return[[:space:]]*"/, "", s)
          sub(/"$/, "", s)
          print s
          exit 0
        }
      }
      in_fn && /}/ { in_fn=0 }
    ' "$SCENARIO_SRC" 2>/dev/null | head -n 1
  )"
  if [[ -z "${out_rel:-}" ]]; then
    echo "fatal: could not infer gc_scenario_out_path() from: $SCENARIO_SRC" >&2
    exit 2
  fi

  actual_path="$(cd "$scenario_dir" && python3 -c 'import os,sys; print(os.path.abspath(sys.argv[1]))' "$out_rel")"
  expected_path="$(python3 -c 'import sys; p=sys.argv[1]; print(p.replace("/actual/","/expected/"))' "$actual_path")"

  if [[ ! -f "$actual_path" ]]; then
    echo "fatal: scenario did not produce actual output: $actual_path" >&2
    exit 2
  fi

  if [[ ! -f "$expected_path" ]]; then
    # Expected fixtures are often derived artifacts and may live in the main repo
    # checkout rather than this worktree.
    if [[ -n "${GC_MAIN_REPO_ROOT:-}" ]]; then
      rel="${expected_path#"$repo_root"/}"
      alt="$GC_MAIN_REPO_ROOT/$rel"
      if [[ -f "$alt" ]]; then
        expected_path="$alt"
      fi
    fi
  fi
  if [[ ! -f "$expected_path" ]]; then
    echo "fatal: expected fixture missing: $expected_path" >&2
    echo "hint: generate expected bins first (Dolphin), then rerun mutation checks." >&2
    echo "hint: or set GC_MAIN_REPO_ROOT to a checkout that has expected/ fixtures." >&2
    exit 2
  fi

  echo "[host-compare] $expected_path vs $actual_path" >&2
  if [[ -x "$repo_root/tools/diff_bins.sh" ]]; then
    "$repo_root/tools/diff_bins.sh" "$expected_path" "$actual_path"
  else
    cmp -s "$expected_path" "$actual_path" || exit 1
  fi
fi

rm -f "$exe" >/dev/null 2>&1 || true
