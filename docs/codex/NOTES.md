# Codex Notes (Facts Only)

Codex has no memory. The repository is the memory.

Rules:
- Facts only. No guesses.
- Every bullet must cite evidence (decomp path + symbol, or a test id + expected.bin).
- Separate: API contract vs internal implementation vs side effects.

## Confirmed Behaviors

### Dolphin GDB Stub (macOS build on this machine)
- Stop packets include PC/NIP in reg `0x40` (usable for PC-polling checkpoints).
  Evidence: `tools/ram_dump.py` `parse_stop_pc()`; observed stop example `T0540:800ba2f0;01:8019d798;` from real MP4 RVZ.
- GDB remote breakpoint packets (`Z0`/`Z1`) are accepted by the stub; `--breakpoint` works.
  Evidence: `tools/ram_dump.py --breakpoint 0x800057c0` hit and returned stop `T0540:800057c0;...` during real MP4 RVZ run.
- Previous "Z0/Z1 not supported" conclusion was a bug in our client: it ignored the `OK` reply because it starts with `O`.
  Evidence: `tools/ram_dump.py` fix after commit `c19d1f5`.

### VISetNextFrameBuffer
- Callsites enumerated across MP4/TP/WW/AC.
  Evidence: docs/sdk/vi/VISetNextFrameBuffer.md
- Current DOL tests in this repo are scaffolding only (marker/control-flow).
  They do not yet validate real VI side effects (shadow regs / HorVer / HW regs).
  Evidence: tests/sdk/vi/vi_set_next_frame_buffer/dol/*; tests/sdk/vi/vi_set_next_frame_buffer/expected/vi_set_next_frame_buffer_{min,realistic_mp4,edge_unaligned}_001.bin

### VIConfigure
- The deterministic oracle for VIConfigure in this repo records: `changeMode`, `dispPosX`, `dispPosY`, `dispSizeX`, `fbSizeX`, `fbSizeY`, `xfbMode`, and `helper_calls`.
  Evidence: tests/sdk/vi/vi_configure/expected/vi_configure_001.bin; tests/sdk/vi/vi_configure/dol/generic/legacy_001/vi_configure_001.c
- Host sdk_port implementation mirrors the legacy testcase's observable behavior (not a full SDK port yet).
  Evidence: src/sdk_port/vi/VI.c; tests/sdk/vi/vi_configure/actual/vi_configure_001.bin

### VIConfigurePan
- The deterministic oracle for VIConfigurePan records: `panPosX`, `panPosY`, `panSizeX`, `panSizeY`, `dispSizeY`, and `helper_calls` for a VI_NON_INTERLACE + VI_XFBMODE_SF configuration.
  Evidence: tests/sdk/vi/vi_configure_pan/expected/vi_configure_pan_001.bin; tests/sdk/vi/vi_configure_pan/dol/generic/legacy_001/vi_configure_pan_001.c
- Host sdk_port implementation mirrors the legacy testcase's observable behavior (not a full SDK port yet).
  Evidence: src/sdk_port/vi/VI.c; tests/sdk/vi/vi_configure_pan/actual/vi_configure_pan_001.bin

### OSSetArenaLo
- MP4 decomp implementation is a single store: `__OSArenaLo = addr;`.
  Evidence: decomp_mario_party_4/src/dolphin/os/OSArena.c
- Minimal test shows `OSGetArenaLo()` returns the last value passed to `OSSetArenaLo()`.
  Evidence: tests/sdk/os/os_set_arena_lo/expected/os_set_arena_lo_min_001.bin
- Double-call edge case: last write wins.
  Evidence: tests/sdk/os/os_set_arena_lo/expected/os_set_arena_lo_edge_double_call_001.bin
- Realistic fixed-value callsite style (TP OSExec/OSReboot uses 0x81280000) is preserved by `OSSetArenaLo`.
  Evidence: decomp_twilight_princess/src/dolphin/os/OSExec.c; tests/sdk/os/os_set_arena_lo/expected/os_set_arena_lo_realistic_tp_fixed_001.bin
- Additional per-game representative testcases exist for MP4/WW/AC (callsite-style inputs, still testing OSSetArenaLo store behavior).
  Evidence: docs/sdk/os/OSSetArenaLo.md; tests/sdk/os/os_set_arena_lo/expected/os_set_arena_lo_realistic_{mp4_mem_alloc_hi,ww_rounddown_hi,ac_stack_round}_001.bin

### OSSetCurrentHeap
- Contract: sets `__OSCurrHeap` and returns the previous value.
  Evidence: decomp_mario_party_4/src/dolphin/os/OSAlloc.c (`OSSetCurrentHeap`); docs/sdk/os/OSSetCurrentHeap.md
- Minimal deterministic test (heap created then set current heap).
  Evidence: tests/sdk/os/os_set_current_heap/expected/os_set_current_heap_generic_min_001.bin
- MP4 InitMem-style sequence is preserved (after `OSInitAlloc` + `OSCreateHeap`).
  Evidence: src/game_workload/mp4/vendor/src/game/init.c (`InitMem`); tests/sdk/os/os_set_current_heap/expected/os_set_current_heap_mp4_realistic_initmem_001.bin

### OSAllocFromHeap
- Contract: allocates `size` bytes from heap handle, returns pointer to user region (header is 0x20 bytes before the returned pointer). Alignment 32 bytes.
  Evidence: decomp_mario_party_4/src/dolphin/os/OSAlloc.c (`OSAllocFromHeap`); docs/sdk/os/OSAllocFromHeap.md
- Minimal deterministic test: two small allocations (0x20, 0x40).
  Evidence: tests/sdk/os/os_alloc_from_heap/expected/os_alloc_from_heap_generic_min_001.bin
- MP4 HuSysInit-style allocation: `OSAlloc(0x100000)` for GX FIFO.
  Evidence: src/game_workload/mp4/vendor/src/game/init.c (`HuSysInit`); tests/sdk/os/os_alloc_from_heap/expected/os_alloc_from_heap_mp4_realistic_fifo_001.bin

### OSSetArenaHi
- Contract: store `__OSArenaHi`; `OSGetArenaHi()` returns last value.
  Evidence: decomp_mario_party_4/src/dolphin/os/OSArena.c; docs/sdk/os/OSSetArenaHi.md; docs/sdk/os/OSGetArenaHi.md
- Minimal deterministic test: set then get.
  Evidence: tests/sdk/os/os_set_arena_hi/expected/os_set_arena_hi_generic_min_001.bin
- Double-call edge case: last write wins.
  Evidence: tests/sdk/os/os_set_arena_hi/expected/os_set_arena_hi_generic_edge_double_call_001.bin

### GXSetChanCtrl
- Contract: packs a channel control bitfield and writes it to XF regs (`idx+14`, and also 16/17 for `GX_COLOR0A0`/`GX_COLOR1A1`).
  Evidence: decomp_mario_party_4/src/dolphin/gx/GXLight.c (`GXSetChanCtrl`).
- MP4 callsite-style testcase (expected.bin only for now; host port pending).
  Evidence: tests/sdk/gx/gx_set_chan_ctrl/expected/gx_set_chan_ctrl_mp4_init_gx_001.bin

### GXSetChanAmbColor
- Contract: packs RGBA into a 32-bit XF reg (`colIdx+10`) and updates `ambColor[colIdx]`.
  Evidence: decomp_mario_party_4/src/dolphin/gx/GXLight.c (`GXSetChanAmbColor`).
- MP4 callsite-style testcase (expected.bin only for now; host port pending).
  Evidence: tests/sdk/gx/gx_set_chan_amb_color/expected/gx_set_chan_amb_color_mp4_init_gx_001.bin

### GXSetChanMatColor
- Contract: packs RGBA into a 32-bit XF reg (`colIdx+12`) and updates `matColor[colIdx]`.
  Evidence: decomp_mario_party_4/src/dolphin/gx/GXLight.c (`GXSetChanMatColor`).
- MP4 callsite-style testcase (expected.bin only for now; host port pending).
  Evidence: tests/sdk/gx/gx_set_chan_mat_color/expected/gx_set_chan_mat_color_mp4_init_gx_001.bin

### GXSetNumTevStages
- Contract: sets `genMode` field (shift 10, size 4) to `nStages-1` and marks `dirtyState |= 4`.
  Evidence: decomp_mario_party_4/src/dolphin/gx/GXTev.c (`GXSetNumTevStages`).
- MP4 callsite-style testcase (expected.bin only for now; host port pending).
  Evidence: tests/sdk/gx/gx_set_num_tev_stages/expected/gx_set_num_tev_stages_mp4_init_gx_001.bin

### GXSetTevOp
- Contract: expands a "mode" into calls to `GXSetTevColorIn/AlphaIn` and then sets `GXSetTevColorOp/AlphaOp` (stage 0: `GX_REPLACE` uses TEX).
  Evidence: decomp_mario_party_4/src/dolphin/gx/GXTev.c (`GXSetTevOp`).
- MP4 callsite-style testcase (expected.bin only for now; host port pending).
  Evidence: tests/sdk/gx/gx_set_tev_op/expected/gx_set_tev_op_mp4_init_gx_001.bin

### GXSetAlphaCompare
- Contract: packs refs/comps/op into a RAS reg with high byte `0xF3` and writes it.
  Evidence: decomp_mario_party_4/src/dolphin/gx/GXTev.c (`GXSetAlphaCompare`).
- MP4 callsite-style testcase (expected.bin only for now; host port pending).
  Evidence: tests/sdk/gx/gx_set_alpha_compare/expected/gx_set_alpha_compare_mp4_init_gx_001.bin

### GXSetBlendMode
- Contract: updates `cmode0` fields for blend/logical/subtract, sets opcode and factors, sets high byte `0x41`, writes RAS reg.
  Evidence: decomp_mario_party_4/src/dolphin/gx/GXPixel.c (`GXSetBlendMode`).
- MP4 callsite-style testcase (expected.bin only for now; host port pending).
  Evidence: tests/sdk/gx/gx_set_blend_mode/expected/gx_set_blend_mode_mp4_init_gx_001.bin

### GXSetAlphaUpdate
- Contract: updates `cmode0` bit 4, writes RAS reg.
  Evidence: decomp_mario_party_4/src/dolphin/gx/GXPixel.c (`GXSetAlphaUpdate`).
- MP4 callsite-style testcase (expected.bin only for now; host port pending).
  Evidence: tests/sdk/gx/gx_set_alpha_update/expected/gx_set_alpha_update_mp4_init_gx_001.bin

### GXSetZCompLoc
- Contract: updates `peCtrl` bit 6, writes RAS reg.
  Evidence: decomp_mario_party_4/src/dolphin/gx/GXPixel.c (`GXSetZCompLoc`).
- MP4 callsite-style testcase (expected.bin only for now; host port pending).
  Evidence: tests/sdk/gx/gx_set_z_comp_loc/expected/gx_set_z_comp_loc_mp4_init_gx_001.bin

### GXSetDither
- Contract: updates `cmode0` bit 2, writes RAS reg.
  Evidence: decomp_mario_party_4/src/dolphin/gx/GXPixel.c (`GXSetDither`).
- MP4 callsite-style testcase (expected.bin only for now; host port pending).
  Evidence: tests/sdk/gx/gx_set_dither/expected/gx_set_dither_mp4_init_gx_001.bin

## Known Invariants

## Undocumented Quirks

## Test Runs (auto)

- Format: `[PASS|FAIL] <label> expected=<path> actual=<path> (first_mismatch=0x........)`
- [PASS] 2026-02-06T09:54:20Z OSSetArenaLo/min_001 expected=tests/sdk/os/os_set_arena_lo/expected/os_set_arena_lo_min_001.bin actual=tests/sdk/os/os_set_arena_lo/actual/os_set_arena_lo_min_001.bin
- [PASS] 2026-02-06T10:03:35Z OSSetArenaLo/min_001 expected=tests/sdk/os/os_set_arena_lo/expected/os_set_arena_lo_min_001.bin actual=tests/sdk/os/os_set_arena_lo/actual/os_set_arena_lo_min_001.bin
- [PASS] 2026-02-06T10:06:08Z OSSetArenaLo/os_set_arena_lo_edge_double_call_001 expected=tests/sdk/os/os_set_arena_lo/expected/os_set_arena_lo_edge_double_call_001.bin actual=tests/sdk/os/os_set_arena_lo/actual/os_set_arena_lo_edge_double_call_001.bin
- [PASS] 2026-02-06T10:06:08Z OSSetArenaLo/os_set_arena_lo_realistic_tp_fixed_001 expected=tests/sdk/os/os_set_arena_lo/expected/os_set_arena_lo_realistic_tp_fixed_001.bin actual=tests/sdk/os/os_set_arena_lo/actual/os_set_arena_lo_realistic_tp_fixed_001.bin
- [PASS] 2026-02-06T10:06:08Z OSSetArenaLo/os_set_arena_lo_realistic_mp4_mem_alloc_hi_001 expected=tests/sdk/os/os_set_arena_lo/expected/os_set_arena_lo_realistic_mp4_mem_alloc_hi_001.bin actual=tests/sdk/os/os_set_arena_lo/actual/os_set_arena_lo_realistic_mp4_mem_alloc_hi_001.bin
- [PASS] 2026-02-06T10:06:08Z OSSetArenaLo/os_set_arena_lo_realistic_ww_rounddown_hi_001 expected=tests/sdk/os/os_set_arena_lo/expected/os_set_arena_lo_realistic_ww_rounddown_hi_001.bin actual=tests/sdk/os/os_set_arena_lo/actual/os_set_arena_lo_realistic_ww_rounddown_hi_001.bin
- [PASS] 2026-02-06T10:06:08Z OSSetArenaLo/os_set_arena_lo_realistic_ac_stack_round_001 expected=tests/sdk/os/os_set_arena_lo/expected/os_set_arena_lo_realistic_ac_stack_round_001.bin actual=tests/sdk/os/os_set_arena_lo/actual/os_set_arena_lo_realistic_ac_stack_round_001.bin
- [PASS] 2026-02-06T10:18:27Z OSGetArenaLo/os_get_arena_lo_min_001 expected=tests/sdk/os/os_get_arena_lo/expected/os_get_arena_lo_min_001.bin actual=tests/sdk/os/os_get_arena_lo/actual/os_get_arena_lo_min_001.bin
- [PASS] 2026-02-06T10:18:27Z OSGetArenaLo/os_get_arena_lo_edge_default_uninit_001 expected=tests/sdk/os/os_get_arena_lo/expected/os_get_arena_lo_edge_default_uninit_001.bin actual=tests/sdk/os/os_get_arena_lo/actual/os_get_arena_lo_edge_default_uninit_001.bin
- [PASS] 2026-02-06T10:18:27Z OSGetArenaLo/os_get_arena_lo_realistic_mem_alloc_001 expected=tests/sdk/os/os_get_arena_lo/expected/os_get_arena_lo_realistic_mem_alloc_001.bin actual=tests/sdk/os/os_get_arena_lo/actual/os_get_arena_lo_realistic_mem_alloc_001.bin
- [PASS] 2026-02-06T10:18:27Z OSGetArenaLo/os_get_arena_lo_realistic_stack_round_001 expected=tests/sdk/os/os_get_arena_lo/expected/os_get_arena_lo_realistic_stack_round_001.bin actual=tests/sdk/os/os_get_arena_lo/actual/os_get_arena_lo_realistic_stack_round_001.bin
- [PASS] 2026-02-06T11:22:25Z OSInitAlloc/os_init_alloc_ac_maxheaps_001 expected=tests/sdk/os/os_init_alloc/expected/os_init_alloc_ac_maxheaps_001.bin actual=tests/sdk/os/os_init_alloc/actual/os_init_alloc_ac_maxheaps_001.bin
- [PASS] 2026-02-06T11:22:25Z OSInitAlloc/os_init_alloc_edge_unaligned_001 expected=tests/sdk/os/os_init_alloc/expected/os_init_alloc_edge_unaligned_001.bin actual=tests/sdk/os/os_init_alloc/actual/os_init_alloc_edge_unaligned_001.bin
- [PASS] 2026-02-06T11:22:25Z OSInitAlloc/os_init_alloc_min_001 expected=tests/sdk/os/os_init_alloc/expected/os_init_alloc_min_001.bin actual=tests/sdk/os/os_init_alloc/actual/os_init_alloc_min_001.bin
- [PASS] 2026-02-06T11:22:25Z OSInitAlloc/os_init_alloc_mp4_realistic_initmem_001 expected=tests/sdk/os/os_init_alloc/expected/os_init_alloc_mp4_realistic_initmem_001.bin actual=tests/sdk/os/os_init_alloc/actual/os_init_alloc_mp4_realistic_initmem_001.bin
- [PASS] 2026-02-06T11:22:25Z OSInitAlloc/os_init_alloc_tp_maxheaps_001 expected=tests/sdk/os/os_init_alloc/expected/os_init_alloc_tp_maxheaps_001.bin actual=tests/sdk/os/os_init_alloc/actual/os_init_alloc_tp_maxheaps_001.bin
- [PASS] 2026-02-06T11:22:25Z OSInitAlloc/os_init_alloc_ww_maxheaps_001 expected=tests/sdk/os/os_init_alloc/expected/os_init_alloc_ww_maxheaps_001.bin actual=tests/sdk/os/os_init_alloc/actual/os_init_alloc_ww_maxheaps_001.bin
- [PASS] 2026-02-06T11:38:40Z OSCreateHeap/os_create_heap_mp4_realistic_initmem_001 expected=tests/sdk/os/os_create_heap/expected/os_create_heap_mp4_realistic_initmem_001.bin actual=tests/sdk/os/os_create_heap/actual/os_create_heap_mp4_realistic_initmem_001.bin
- [PASS] 2026-02-06T11:38:40Z OSCreateHeap/os_create_heap_ww_jkrstdheap_001 expected=tests/sdk/os/os_create_heap/expected/os_create_heap_ww_jkrstdheap_001.bin actual=tests/sdk/os/os_create_heap/actual/os_create_heap_ww_jkrstdheap_001.bin
- [PASS] 2026-02-06T12:09:45Z OSSetCurrentHeap/generic_min_001 expected=tests/sdk/os/os_set_current_heap/expected/os_set_current_heap_generic_min_001.bin actual=tests/sdk/os/os_set_current_heap/actual/os_set_current_heap_generic_min_001.bin
- [PASS] 2026-02-06T12:09:45Z OSSetCurrentHeap/mp4_realistic_initmem_001 expected=tests/sdk/os/os_set_current_heap/expected/os_set_current_heap_mp4_realistic_initmem_001.bin actual=tests/sdk/os/os_set_current_heap/actual/os_set_current_heap_mp4_realistic_initmem_001.bin
- [PASS] 2026-02-06T12:16:08Z OSAllocFromHeap/mp4_realistic_fifo_001 expected=tests/sdk/os/os_alloc_from_heap/expected/os_alloc_from_heap_mp4_realistic_fifo_001.bin actual=tests/sdk/os/os_alloc_from_heap/actual/os_alloc_from_heap_mp4_realistic_fifo_001.bin
- [PASS] 2026-02-06T12:24:14Z OSSetArenaHi/edge_double_call_001 expected=tests/sdk/os/os_set_arena_hi/expected/os_set_arena_hi_generic_edge_double_call_001.bin actual=tests/sdk/os/os_set_arena_hi/actual/os_set_arena_hi_generic_edge_double_call_001.bin
- [PASS] 2026-02-06T19:13:29Z GX batch3/mp4_init_gx_001/gx_set_chan_ctrl expected=tests/sdk/gx/gx_set_chan_ctrl/expected/gx_set_chan_ctrl_mp4_init_gx_001.bin actual=tests/sdk/gx/gx_set_chan_ctrl/actual/gx_set_chan_ctrl_mp4_init_gx_001.bin
- [PASS] 2026-02-06T19:13:30Z GX batch3/mp4_init_gx_001/gx_set_chan_amb_color expected=tests/sdk/gx/gx_set_chan_amb_color/expected/gx_set_chan_amb_color_mp4_init_gx_001.bin actual=tests/sdk/gx/gx_set_chan_amb_color/actual/gx_set_chan_amb_color_mp4_init_gx_001.bin
- [PASS] 2026-02-06T19:13:30Z GX batch3/mp4_init_gx_001/gx_set_chan_mat_color expected=tests/sdk/gx/gx_set_chan_mat_color/expected/gx_set_chan_mat_color_mp4_init_gx_001.bin actual=tests/sdk/gx/gx_set_chan_mat_color/actual/gx_set_chan_mat_color_mp4_init_gx_001.bin
- [PASS] 2026-02-06T19:13:30Z GX batch3/mp4_init_gx_001/gx_set_num_tev_stages expected=tests/sdk/gx/gx_set_num_tev_stages/expected/gx_set_num_tev_stages_mp4_init_gx_001.bin actual=tests/sdk/gx/gx_set_num_tev_stages/actual/gx_set_num_tev_stages_mp4_init_gx_001.bin
- [PASS] 2026-02-06T19:13:30Z GX batch3/mp4_init_gx_001/gx_set_tev_op expected=tests/sdk/gx/gx_set_tev_op/expected/gx_set_tev_op_mp4_init_gx_001.bin actual=tests/sdk/gx/gx_set_tev_op/actual/gx_set_tev_op_mp4_init_gx_001.bin
- [PASS] 2026-02-06T19:13:30Z GX batch3/mp4_init_gx_001/gx_set_alpha_compare expected=tests/sdk/gx/gx_set_alpha_compare/expected/gx_set_alpha_compare_mp4_init_gx_001.bin actual=tests/sdk/gx/gx_set_alpha_compare/actual/gx_set_alpha_compare_mp4_init_gx_001.bin
- [PASS] 2026-02-06T19:13:30Z GX batch3/mp4_init_gx_001/gx_set_blend_mode expected=tests/sdk/gx/gx_set_blend_mode/expected/gx_set_blend_mode_mp4_init_gx_001.bin actual=tests/sdk/gx/gx_set_blend_mode/actual/gx_set_blend_mode_mp4_init_gx_001.bin
- [PASS] 2026-02-06T19:13:30Z GX batch3/mp4_init_gx_001/gx_set_alpha_update expected=tests/sdk/gx/gx_set_alpha_update/expected/gx_set_alpha_update_mp4_init_gx_001.bin actual=tests/sdk/gx/gx_set_alpha_update/actual/gx_set_alpha_update_mp4_init_gx_001.bin
- [PASS] 2026-02-06T19:13:30Z GX batch3/mp4_init_gx_001/gx_set_z_comp_loc expected=tests/sdk/gx/gx_set_z_comp_loc/expected/gx_set_z_comp_loc_mp4_init_gx_001.bin actual=tests/sdk/gx/gx_set_z_comp_loc/actual/gx_set_z_comp_loc_mp4_init_gx_001.bin
- [PASS] 2026-02-06T19:13:30Z GX batch3/mp4_init_gx_001/gx_set_dither expected=tests/sdk/gx/gx_set_dither/expected/gx_set_dither_mp4_init_gx_001.bin actual=tests/sdk/gx/gx_set_dither/actual/gx_set_dither_mp4_init_gx_001.bin
