#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"

# Usage:
#   tools/run_pbt.sh [suite] [iters] [seed]
# Suites:
#   - os_round_32b (default)
#   - os_arena
#   - os_interrupts
#   - os_module_queue
#   - si_sampling_rate
#   - si_get_response
#   - si_transfer
#   - pad_control_motor
#   - vi_post_retrace_callback
#   - pad_reset_clamp
#   - dvd_read_prio
#   - dvd_read_async_prio
#   - dvd_convert_path
#   - gx_texcopy_relation
#   - gx_vtxdesc_packing
#   - gx_alpha_tev_packing
#   - mtx_core
#   - dvd_core

suite="${1:-os_round_32b}"
iters="${2:-200000}"
seed="${3:-0xC0DEC0DE}"

if [[ "$suite" =~ ^[0-9]+$ ]]; then
  # Backward compatibility with old positional form: run_pbt.sh <iters> <seed>
  seed="${2:-0xC0DEC0DE}"
  iters="$suite"
  suite="os_round_32b"
fi

build_dir="$repo_root/tests/build/pbt"
mkdir -p "$build_dir"

case "$suite" in
  os_round_32b)
    cc -O2 -g0 \
      -I"$repo_root/src" -I"$repo_root/src/sdk_port" \
      "$repo_root/src/sdk_port/gc_mem.c" \
      "$repo_root/src/sdk_port/os/OSArena.c" \
      "$repo_root/tests/pbt/os/os_round_32b/os_round_32b_pbt.c" \
      -o "$build_dir/os_round_32b_pbt"
    "$build_dir/os_round_32b_pbt" "$iters" "$seed"
    ;;
  os_arena)
    cc -O2 -g0 \
      -I"$repo_root/src" -I"$repo_root/src/sdk_port" \
      "$repo_root/src/sdk_port/gc_mem.c" \
      "$repo_root/src/sdk_port/os/OSArena.c" \
      "$repo_root/tests/pbt/os/os_arena/os_arena_pbt.c" \
      -o "$build_dir/os_arena_pbt"
    "$build_dir/os_arena_pbt" "$iters" "$seed"
    ;;
  os_interrupts)
    cc -O2 -g0 \
      -I"$repo_root/src" -I"$repo_root/src/sdk_port" \
      "$repo_root/src/sdk_port/gc_mem.c" \
      "$repo_root/src/sdk_port/os/OSInterrupts.c" \
      "$repo_root/tests/pbt/os/os_interrupts/os_interrupts_pbt.c" \
      -o "$build_dir/os_interrupts_pbt"
    "$build_dir/os_interrupts_pbt" "$iters" "$seed"
    ;;
  os_module_queue)
    cc -O2 -g0 \
      -I"$repo_root/src" -I"$repo_root/src/sdk_port" -I"$repo_root/tests/workload/include" \
      "$repo_root/src/sdk_port/os/OSModule.c" \
      "$repo_root/tests/pbt/os/os_module_queue/os_module_queue_pbt.c" \
      -o "$build_dir/os_module_queue_pbt"
    "$build_dir/os_module_queue_pbt" "$iters" "$seed"
    ;;
  si_sampling_rate)
    cc -O2 -g0 \
      -I"$repo_root/src" -I"$repo_root/src/sdk_port" -I"$repo_root/tests/workload/include" \
      "$repo_root/src/sdk_port/gc_mem.c" \
      "$repo_root/src/sdk_port/os/OSInterrupts.c" \
      "$repo_root/src/sdk_port/vi/VI.c" \
      "$repo_root/src/sdk_port/si/SI.c" \
      "$repo_root/src/sdk_port/os/OSError.c" \
      "$repo_root/tests/pbt/si/si_sampling_rate/si_sampling_rate_pbt.c" \
      -o "$build_dir/si_sampling_rate_pbt"
    "$build_dir/si_sampling_rate_pbt" "$iters" "$seed"
    ;;
  si_get_response)
    cc -O2 -g0 \
      -I"$repo_root/src" -I"$repo_root/src/sdk_port" -I"$repo_root/tests/workload/include" \
      "$repo_root/src/sdk_port/gc_mem.c" \
      "$repo_root/src/sdk_port/os/OSInterrupts.c" \
      "$repo_root/src/sdk_port/vi/VI.c" \
      "$repo_root/src/sdk_port/si/SI.c" \
      "$repo_root/src/sdk_port/os/OSError.c" \
      "$repo_root/tests/pbt/si/si_get_response/si_get_response_pbt.c" \
      -o "$build_dir/si_get_response_pbt"
    "$build_dir/si_get_response_pbt" "$iters" "$seed"
    ;;
  si_transfer)
    cc -O2 -g0 \
      -I"$repo_root/src" -I"$repo_root/src/sdk_port" -I"$repo_root/tests/workload/include" \
      "$repo_root/src/sdk_port/gc_mem.c" \
      "$repo_root/src/sdk_port/os/OSInterrupts.c" \
      "$repo_root/src/sdk_port/vi/VI.c" \
      "$repo_root/src/sdk_port/si/SI.c" \
      "$repo_root/src/sdk_port/os/OSError.c" \
      "$repo_root/tests/pbt/si/si_transfer/si_transfer_pbt.c" \
      -o "$build_dir/si_transfer_pbt"
    "$build_dir/si_transfer_pbt" "$iters" "$seed"
    ;;
  pad_control_motor)
    cc -O2 -g0 \
      -I"$repo_root/src" -I"$repo_root/src/sdk_port" -I"$repo_root/tests/workload/include" \
      "$repo_root/src/sdk_port/gc_mem.c" \
      "$repo_root/src/sdk_port/pad/PAD.c" \
      "$repo_root/src/sdk_port/si/SI.c" \
      "$repo_root/src/sdk_port/vi/VI.c" \
      "$repo_root/src/sdk_port/os/OSInterrupts.c" \
      "$repo_root/src/sdk_port/os/OSError.c" \
      "$repo_root/tests/pbt/pad/pad_control_motor/pad_control_motor_pbt.c" \
      -o "$build_dir/pad_control_motor_pbt"
    "$build_dir/pad_control_motor_pbt" "$iters" "$seed"
    ;;
  vi_post_retrace_callback)
    cc -O2 -g0 \
      -I"$repo_root/src" -I"$repo_root/src/sdk_port" -I"$repo_root/tests/workload/include" \
      "$repo_root/src/sdk_port/gc_mem.c" \
      "$repo_root/src/sdk_port/os/OSInterrupts.c" \
      "$repo_root/src/sdk_port/vi/VI.c" \
      "$repo_root/tests/pbt/vi/vi_post_retrace_callback/vi_post_retrace_callback_pbt.c" \
      -o "$build_dir/vi_post_retrace_callback_pbt"
    "$build_dir/vi_post_retrace_callback_pbt" "$iters" "$seed"
    ;;
  pad_reset_clamp)
    cc -O2 -g0 \
      -I"$repo_root/src" -I"$repo_root/src/sdk_port" -I"$repo_root/tests/workload/include" \
      "$repo_root/src/sdk_port/gc_mem.c" \
      "$repo_root/src/sdk_port/pad/PAD.c" \
      "$repo_root/src/sdk_port/si/SI.c" \
      "$repo_root/src/sdk_port/vi/VI.c" \
      "$repo_root/src/sdk_port/os/OSInterrupts.c" \
      "$repo_root/src/sdk_port/os/OSError.c" \
      "$repo_root/tests/pbt/pad/pad_reset_clamp/pad_reset_clamp_pbt.c" \
      -o "$build_dir/pad_reset_clamp_pbt"
    "$build_dir/pad_reset_clamp_pbt" "$iters" "$seed"
    ;;
  dvd_read_prio)
    cc -O2 -g0 \
      -I"$repo_root/src" -I"$repo_root/src/sdk_port" \
      "$repo_root/src/sdk_port/gc_mem.c" \
      "$repo_root/src/sdk_port/dvd/DVD.c" \
      "$repo_root/tests/pbt/dvd/dvd_read_prio/dvd_read_prio_pbt.c" \
      -o "$build_dir/dvd_read_prio_pbt"
    "$build_dir/dvd_read_prio_pbt" "$iters" "$seed"
    ;;
  dvd_read_async_prio)
    cc -O2 -g0 \
      -I"$repo_root/src" -I"$repo_root/src/sdk_port" \
      "$repo_root/src/sdk_port/gc_mem.c" \
      "$repo_root/src/sdk_port/dvd/DVD.c" \
      "$repo_root/tests/pbt/dvd/dvd_read_async_prio/dvd_read_async_prio_pbt.c" \
      -o "$build_dir/dvd_read_async_prio_pbt"
    "$build_dir/dvd_read_async_prio_pbt" "$iters" "$seed"
    ;;
  dvd_convert_path)
    cc -O2 -g0 \
      -I"$repo_root/src" -I"$repo_root/src/sdk_port" \
      "$repo_root/src/sdk_port/gc_mem.c" \
      "$repo_root/src/sdk_port/dvd/DVD.c" \
      "$repo_root/tests/pbt/dvd/dvd_convert_path/dvd_convert_path_pbt.c" \
      -o "$build_dir/dvd_convert_path_pbt"
    "$build_dir/dvd_convert_path_pbt" "$iters" "$seed"
    ;;
  gx_texcopy_relation)
    cc -O2 -g0 \
      -I"$repo_root/src" -I"$repo_root/src/sdk_port" -I"$repo_root/tests/workload/include" \
      "$repo_root/src/sdk_port/gc_mem.c" \
      "$repo_root/src/sdk_port/gx/GX.c" \
      "$repo_root/tests/pbt/gx/gx_texcopy_relation/gx_texcopy_relation_pbt.c" \
      -o "$build_dir/gx_texcopy_relation_pbt"
    "$build_dir/gx_texcopy_relation_pbt" "$iters" "$seed"
    ;;
  gx_vtxdesc_packing)
    cc -O2 -g0 \
      -I"$repo_root/src" -I"$repo_root/src/sdk_port" -I"$repo_root/tests/workload/include" \
      "$repo_root/src/sdk_port/gc_mem.c" \
      "$repo_root/src/sdk_port/gx/GX.c" \
      "$repo_root/tests/pbt/gx/gx_vtxdesc_packing/gx_vtxdesc_packing_pbt.c" \
      -o "$build_dir/gx_vtxdesc_packing_pbt"
    "$build_dir/gx_vtxdesc_packing_pbt" "$iters" "$seed"
    ;;
  gx_alpha_tev_packing)
    cc -O2 -g0 \
      -I"$repo_root/src" -I"$repo_root/src/sdk_port" -I"$repo_root/tests/workload/include" \
      "$repo_root/src/sdk_port/gc_mem.c" \
      "$repo_root/src/sdk_port/gx/GX.c" \
      "$repo_root/tests/pbt/gx/gx_alpha_tev_packing/gx_alpha_tev_packing_pbt.c" \
      -o "$build_dir/gx_alpha_tev_packing_pbt"
    "$build_dir/gx_alpha_tev_packing_pbt" "$iters" "$seed"
    ;;
  mtx_core)
    cc -O2 -g0 \
      -I"$repo_root/src" -I"$repo_root/src/sdk_port" \
      "$repo_root/src/sdk_port/mtx/mtx.c" \
      "$repo_root/src/sdk_port/mtx/vec.c" \
      "$repo_root/src/sdk_port/mtx/quat.c" \
      "$repo_root/src/sdk_port/mtx/mtx44.c" \
      "$repo_root/tests/pbt/mtx/mtx_core_pbt.c" \
      -lm \
      -o "$build_dir/mtx_core_pbt"
    "$build_dir/mtx_core_pbt" "$iters" "$seed"
    ;;
  dvd_core)
    cc -O2 -g0 \
      -I"$repo_root/src" -I"$repo_root/src/sdk_port" \
      "$repo_root/src/sdk_port/gc_mem.c" \
      "$repo_root/src/sdk_port/dvd/DVD.c" \
      "$repo_root/tests/pbt/dvd/dvd_core_pbt.c" \
      -o "$build_dir/dvd_core_pbt"
    "$build_dir/dvd_core_pbt" "$iters" "$seed"
    ;;
  *)
    echo "Unknown PBT suite: $suite" >&2
    exit 2
    ;;
esac
