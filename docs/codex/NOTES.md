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
- Real MP4 RVZ breakpoint test (USA): breakpoint at `OSDisableInterrupts` address `0x800B723C` hits and stops (`T0540:800b723c;...`).
- MP4 post-`HuPadInit` init continues with `HuPerfInit` (perf.c). New SDK calls covered with deterministic PPC-vs-host tests:
  - `OSInitStopwatch` (from decomp `src/dolphin/os/OSStopwatch.c`: sets name/total/hits/min/max only; does not touch running/last).
  - `GXSetDrawSyncCallback` / `GXSetDrawSync` (from decomp `src/dolphin/gx/GXMisc.c`).
  - Suites: `tests/sdk/os/os_init_stopwatch`, `tests/sdk/gx/gx_set_draw_sync_callback`, `tests/sdk/gx/gx_set_draw_sync`.
  Evidence: `/tmp/mp4_rvz_osdisable_bp_0x80300000.bin` produced by `tools/ram_dump.py --breakpoint 0x800B723C` on `/Users/chrislamark/projects/recomp/gamecube_static_recomp/game_files/Mario Party 4 (USA).rvz`

### MP4 checkpoint smoke chain: mp4_pad_init_chain_001
- Purpose: validate combined SDK effects for the HuPadInit-related subset (SI/VI/PAD + OS interrupt gating) as a checkpoint dump.
- OSDisableInterrupts MP4-realistic test suite: `tests/sdk/os/os_disable_interrupts` now PASSes expected vs actual.
  - Fix applied: DOL Makefile must not include decomp headers, because some decomps ship a nonstandard `<stdint.h>` that breaks `uint32_t` etc.
  Evidence: `tests/sdk/smoke/mp4_pad_init_chain_001/README.md`
- Determinism scope:
  - Primary oracle: `sdk_port` on PPC (Dolphin DOL) vs `sdk_port` on host (virtual RAM) should be bit-exact for the dumped ranges.
  - This does NOT prove retail MP4 correctness; retail correctness needs the RVZ breakpoint dump oracle.
  Evidence: `docs/codex/WORKFLOW.md` ("Checkpoint Dumps")
- Dolphin warning seen sometimes when running tiny DOLs: "Invalid write to 0x00000900" / "Unknown instruction at PC=00000900".
  Interpretation: PPC exception vector `0x00000900` (decrementer/interrupt path) firing before handlers are installed.
  Mitigation in smoke DOLs: use `gc_safepoint()` (disable MSR[EE] + push DEC far future) very early and during loops.
  Evidence: `tests/sdk/smoke/mp4_pad_init_chain_001/dol/mp4/mp4_pad_init_chain_001/mp4_pad_init_chain_001.c`

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

### GXSetCopyClear (MP4 Hu3DPreProc)
- Contract: emits 3 BP/RAS writes (0x4F/0x50/0x51) encoding clear color (R,A then B,G) and 24-bit Z.
  Evidence: decomp_mario_party_4/src/dolphin/gx/GXFrameBuf.c (`GXSetCopyClear`).
- MP4 callsite test: `GXSetCopyClear(BGColor={0,0,0,0xFF}, 0xFFFFFF)`.
  Evidence: decomp_mario_party_4/src/game/hsfman.c (`Hu3DPreProc`); tests/sdk/gx/gx_set_copy_clear/expected/gx_set_copy_clear_mp4_hu3d_preproc_001.bin

### GXSetFog (MP4 Hu3DFogClear / shadow path)
- MP4 callsite style (fog clear): `GXSetFog(GX_FOG_NONE, 0,0,0,0, BGColor)` updates fog BP regs 0xEE..0xF2 (fog0..fog3 + fogclr) and writes them as last RAS reg sequence.
  Evidence: decomp_mario_party_4/src/game/hsfman.c (`Hu3DShadowExec` fog clear); decomp_mario_party_4/src/dolphin/gx/GXPixel.c (`GXSetFog`);
  tests/sdk/gx/gx_set_fog/expected/gx_set_fog_mp4_hu3d_fog_clear_001.bin

### GXSetProjection (MP4 Hu3DShadowExec ortho)
- MP4 shadow-copy path sets an orthographic projection after copying the shadow texture:
  `C_MTXOrtho(sp18, 0,1,0,1,0,1); GXSetProjection(sp18, GX_ORTHOGRAPHIC);`
  Evidence: decomp_mario_party_4/src/game/hsfman.c (`Hu3DShadowExec`); tests/sdk/gx/gx_set_projection/expected/gx_set_projection_mp4_shadow_ortho_001.bin
- Observable side effects (deterministic oracle):
  - projType stored
  - projMtx[0..5] packed from the input matrix
  - XF regs 32..38 written (32..37 = projMtx floats, 38 = projType)
  Evidence: decomp_mario_party_4/src/dolphin/gx/GXTransform.c (`GXSetProjection`/`__GXSetProjection`); tests/sdk/gx/gx_set_projection/expected/gx_set_projection_mp4_shadow_ortho_001.bin

### GXSetTexCopySrc (MP4 Hu3DShadowExec small branch)
- MP4 callsite (shadow copy setup, unk_02 <= 0xF0):
  `GXSetTexCopySrc(0, 0, unk_02 * 2, unk_02 * 2)` packs:
  - `cpTexSrc` high byte 0x49 with left/top fields
  - `cpTexSize` high byte 0x4A with (wd-1)/(ht-1)
  Evidence: decomp_mario_party_4/src/game/hsfman.c (`Hu3DShadowExec`); decomp_mario_party_4/src/dolphin/gx/GXFrameBuf.c (`GXSetTexCopySrc`);
  tests/sdk/gx/gx_set_tex_copy_src/expected/gx_set_tex_copy_src_mp4_shadow_small_001.bin

### GXSetTexCopyDst (MP4 Hu3DShadowExec small branch)
- MP4 callsite (shadow copy setup, unk_02 <= 0xF0):
  `GXSetTexCopyDst(unk_02, unk_02, GX_CTF_R8, 1)` packs:
  - `cpTexStride` high byte 0x4D, field0 = `rowTiles * cmpTiles` for format
  - `cpTex` mipmap bit + format fields (used later by `GXCopyTex`)
  - `cpTexZ` flag (0 for GX_CTF_R8)
  Evidence: decomp_mario_party_4/src/game/hsfman.c (`Hu3DShadowExec`); decomp_mario_party_4/src/dolphin/gx/GXFrameBuf.c (`GXSetTexCopyDst`);
  tests/sdk/gx/gx_set_tex_copy_dst/expected/gx_set_tex_copy_dst_mp4_shadow_small_001.bin

### GXCopyTex (MP4 Hu3DShadowExec small branch)
- MP4 callsite (shadow copy):
  `GXCopyTex(dest, GX_TRUE)` writes:
  - BP 0x4B (addr): low 21 bits = `(dest & 0x3FFFFFFF) >> 5`
  - BP 0x52 (cpTex): sets clear bit (bit11) and tex-copy bit14=0
  - On `clear=1`, also emits zmode (0x40...) then a temporary cmode reg (0x42...) with bits0/1 cleared.
  Evidence: decomp_mario_party_4/src/game/hsfman.c (`Hu3DShadowExec`); decomp_mario_party_4/src/dolphin/gx/GXFrameBuf.c (`GXCopyTex`);
  tests/sdk/gx/gx_copy_tex/expected/gx_copy_tex_mp4_shadow_small_001.bin

### GXGetTexBufferSize (MP4 WipeCrossFade)
- MP4 callsite:
  `GXGetTexBufferSize(w, h, GX_TF_RGB565, GX_FALSE, 0)` where `w=fbWidth=640`, `h=efbHeight=480`.
  Returns `0x00096000` bytes for RGB565 (tile shift 2x2, tileBytes=32).
  Evidence: decomp_mario_party_4/src/game/wipe.c (`WipeInit`, `WipeCrossFade`);
  decomp_mario_party_4/src/dolphin/gx/GXTexture.c (`GXGetTexBufferSize`);
  tests/sdk/gx/gx_get_tex_buffer_size/expected/gx_get_tex_buffer_size_mp4_wipe_cross_fade_001.bin

### GXLoadPosMtxImm (MP4 wipe)
- MP4 callsite (wipe):
  `GXLoadPosMtxImm(identity, GX_PNMTX0)` writes FIFO cmd `0x10`, reg `0x000B0000`, then 12 float words (row-major 3x4).
  Evidence: decomp_mario_party_4/src/game/wipe.c (`WipeFrameStill`); decomp_mario_party_4/src/dolphin/gx/GXTransform.c (`GXLoadPosMtxImm`);
  tests/sdk/gx/gx_load_pos_mtx_imm/expected/gx_load_pos_mtx_imm_mp4_wipe_identity_001.bin

### GXBegin / GXEnd (MP4 wipe)
- MP4 callsite (wipe):
  `GXBegin(GX_QUADS, GX_VTXFMT0, 4)` emits a FIFO header:
  - `GX_WRITE_U8(vtxfmt | type)` -> `0x80`
  - `GX_WRITE_U16(nverts)` -> `4`
  `GXEnd()` is a macro barrier (no observable state change for our deterministic model).
  Evidence: decomp_mario_party_4/src/game/wipe.c (`WipeFrameStill`); decomp_mario_party_4/src/dolphin/gx/GXGeometry.c (`GXBegin`);
  tests/sdk/gx/gx_begin/expected/gx_begin_mp4_wipe_quads_001.bin

### GXSetTexCoordGen (MP4 wipe)
- MP4 callsite (wipe):
  `GXSetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY)` calls `GXSetTexCoordGen2(..., GX_FALSE, GX_PTIDENTITY)`.
  For this callsite, the observable end state is:
  - XF texgen ctrl (0x40 + 0): `0x00000280` (row=5 at bits [7..11])
  - XF postmtx/normalize (0x50 + 0): `0x0000003D` (postmtx=125 => 125-64=61)
  - matIdxA TEX0 field (bits [6..11]) = `GX_IDENTITY` (60), so `matIdxA = 60 << 6 = 0x00000F00`
  - `__GXSetMatrixIndex` writes XF reg 24 = `matIdxA`
  Evidence: decomp_mario_party_4/src/game/wipe.c (`WipeFrameStill`); decomp_mario_party_4/include/dolphin/gx/GXGeometry.h (`GXSetTexCoordGen` inline);
  decomp_mario_party_4/src/dolphin/gx/GXAttr.c (`GXSetTexCoordGen2`);
  tests/sdk/gx/gx_set_tex_coord_gen/expected/gx_set_tex_coord_gen_mp4_wipe_frame_still_001.bin

### GXSetTevColor (MP4 wipe)
- MP4 callsite (wipe):
  `GXSetTevColor(GX_TEVSTAGE1, color)` uses numeric `1`, which matches `GX_TEVREG0` (C0).
  It packs:
  - `regRA` addr `224 + id*2` with 11-bit `r` at bits [0..10] and 11-bit `a` at [12..22]
  - `regBG` addr `225 + id*2` with 11-bit `b` at bits [0..10] and 11-bit `g` at [12..22]
  SDK writes `regRA` once then `regBG` three times (last write wins).
  Evidence: decomp_mario_party_4/src/game/wipe.c (`WipeFrameStill`); decomp_mario_party_4/src/dolphin/gx/GXTev.c (`GXSetTevColor`);
  tests/sdk/gx/gx_set_tev_color/expected/gx_set_tev_color_mp4_wipe_c0_001.bin

### GXSetCurrentMtx (MP4 Hu3DExec)
- Contract: updates `matIdxA` low 6 bits and writes XF reg 24 to the updated `matIdxA` (via `__GXSetMatrixIndex`).
  Evidence: decomp_mario_party_4/src/dolphin/gx/GXTransform.c (`GXSetCurrentMtx`, `__GXSetMatrixIndex`).
- MP4 callsite test: `GXSetCurrentMtx(0)`.
  Evidence: decomp_mario_party_4/src/game/hsfman.c (`Hu3DExec`); tests/sdk/gx/gx_set_current_mtx/expected/gx_set_current_mtx_mp4_hu3d_exec_001.bin

### GXSetDrawDone / GXWaitDrawDone (MP4 Hu3DExec)
- SDK behavior: `GXSetDrawDone()` writes BP `0x45000002` and resets a completion flag; `GXWaitDrawDone()` blocks until the flag is set by a finish interrupt.
  Evidence: decomp_mario_party_4/src/dolphin/gx/GXMisc.c (`GXSetDrawDone`, `GXWaitDrawDone`).
- Current repo model: deterministic no-block shim for `GXWaitDrawDone` (marks complete immediately) to keep host tests deterministic.
  Evidence: src/sdk_port/gx/GX.c (`GXWaitDrawDone`); tests/sdk/gx/gx_wait_draw_done/expected/gx_wait_draw_done_mp4_hu3d_exec_001.bin

## Known Invariants

## Undocumented Quirks

## Smoke Chains (PPC-vs-Host)

### mp4_init_chain_001 / mp4_init_chain_002
- Fact: Smoke DOLs must compile `sdk_port` modules as separate translation units (`oracle_*.c`) to avoid enum/macro clashes (example: `VI_NTSC` in `VI.c` vs `SI.c`).
  Evidence: `tests/sdk/smoke/mp4_init_chain_001/dol/mp4/mp4_init_chain_001/oracle_*.c`.
- Result: Deterministic compare window passes (marker + snapshot + sdk_state page) for both chain suites.
  Evidence:
  - `tests/sdk/smoke/mp4_init_chain_001/expected/mp4_init_chain_001_mem1.bin` vs `tests/sdk/smoke/mp4_init_chain_001/actual/mp4_init_chain_001_mem1.bin`
  - `tests/sdk/smoke/mp4_init_chain_002/expected/mp4_init_chain_002_mem1.bin` vs `tests/sdk/smoke/mp4_init_chain_002/actual/mp4_init_chain_002_mem1.bin`

### MP4 chain: GXInit tail after GXSetDither
- Fact: Dolphin SDK `GXInit()` continues with additional pixel/copy/POKE setup after `GXSetDither()`.
  Evidence: `decomp_mario_party_4/src/dolphin/gx/GXInit.c:332+` (GXSetDstAlpha, GXSetFieldMask/Mode, GXSetCopyClamp, GXPoke*).
- Result: Added MP4-realistic tests for the missing tail setters/pokes and appended them to `docs/sdk/mp4/MP4_chain_all.csv` so the chain stays contiguous.
  Evidence: `docs/sdk/mp4/MP4_chain_all.csv` PlannedBatch4 (126..147) and `tests/sdk/gx/gx_*` suites.

### Real MP4 RVZ breakpoint sanity check
- Fact: Dolphin GDB stub accepts `Z0/Z1` software breakpoints on this machine.
  Evidence: `tools/ram_dump.py --exec "...Mario Party 4 (USA).rvz" --breakpoint 0x800B723C ...` stops at `T0540:800b723c;...`.
- Fact: RVZ MEM1 dump at PC checkpoint works reliably via polling.
  Evidence: `tools/dump_expected_rvz_mem1_at_pc.sh "...Mario Party 4 (USA).rvz" 0x800B723C tests/oracles/mp4_rvz/mp4_rvz_pc_800B723C_mem1_24m.bin`
  - SHA256: `98316b556ec8c60ad775cba9a1d6048e15347a8cbd9fe1422147091f94a637db`

### DVDConvertPathToEntrynum

- Callsite (MP4): `HuDataInit` validates that a list of `data/*.bin` paths exist, and panics if any returns `-1`.
  Evidence: `decomp_mario_party_4/src/game/data.c`
- Current port status: deterministic test backend only (injectable path table), not a full disc FST model yet.
  Evidence: `src/sdk_port/dvd/DVD.c`
- Testcase:
  - `dvd_convert_path_to_entrynum_mp4_hu_data_init_001`: returns stable indices for known MP4 `data/*.bin` paths; returns `-1` for a missing path.
    Evidence: `tests/sdk/dvd/dvd_convert_path_to_entrynum/expected/dvd_convert_path_to_entrynum_mp4_hu_data_init_001.bin` and `tests/sdk/dvd/dvd_convert_path_to_entrynum/actual/dvd_convert_path_to_entrynum_mp4_hu_data_init_001.bin`
  - `dvd_convert_path_to_entrynum_mp4_datadir_table_001`: validates *all* MP4 `datadir_table.h` paths resolve (count=140); returns `-1` for a missing path.
    Evidence: `tests/sdk/dvd/dvd_convert_path_to_entrynum/expected/dvd_convert_path_to_entrynum_mp4_datadir_table_001.bin` and `tests/sdk/dvd/dvd_convert_path_to_entrynum/actual/dvd_convert_path_to_entrynum_mp4_datadir_table_001.bin`

### DVDFastOpen / DVDReadAsync (MP4 HuDvdDataFastRead path)

- Callsites (MP4):
  - `HuDvdDataFastRead(entrynum)` does `DVDFastOpen(entrynum, &file)` then reads `file.length`.
    Evidence: `decomp_mario_party_4/src/game/dvd.c`
  - `HuDataDVDdirDirectRead(fileInfo, dest, len, offset)` does `DVDReadAsync(...)` then polls `DVDGetCommandBlockStatus(&fileInfo->cb)` until it returns 0.
    Evidence: `decomp_mario_party_4/src/game/data.c`
- Current port status: deterministic “virtual disc” backend only (injectable entrynum->bytes table), not a real disc/FST model yet.
  Evidence: `src/sdk_port/dvd/DVD.c` (`gc_dvd_test_set_file`, `DVDFastOpen`, `DVDReadAsync`)
- Testcases (PPC in Dolphin vs host sdk_port):
  - `dvd_fast_open_mp4_hu_dvd_data_fast_read_001`: `DVDFastOpen(0)` sets `file.length=0x40`.
    Evidence: `tests/sdk/dvd/dvd_fast_open/expected/dvd_fast_open_mp4_hu_dvd_data_fast_read_001.bin` and `tests/sdk/dvd/dvd_fast_open/actual/dvd_fast_open_mp4_hu_dvd_data_fast_read_001.bin`
  - `dvd_read_async_mp4_hu_data_dvd_dir_direct_read_001`: `DVDReadAsync(&fi, dest, 0x20, 0x10, NULL)` copies bytes and `DVDGetCommandBlockStatus(&fi.cb)==0`.
    Evidence: `tests/sdk/dvd/dvd_read_async/expected/dvd_read_async_mp4_hu_data_dvd_dir_direct_read_001.bin` and `tests/sdk/dvd/dvd_read_async/actual/dvd_read_async_mp4_hu_data_dvd_dir_direct_read_001.bin`
  - `dvd_read_async_mp4_hu_dvd_data_read_wait_cb_001`: `DVDReadAsync(&fi, dest, 0x20, 0x00, cb)` calls the callback (cb_called=1) and copies bytes.
    Evidence: `tests/sdk/dvd/dvd_read_async/expected/dvd_read_async_mp4_hu_dvd_data_read_wait_cb_001.bin` and `tests/sdk/dvd/dvd_read_async/actual/dvd_read_async_mp4_hu_dvd_data_read_wait_cb_001.bin`

### DCInvalidateRange (MP4 HuDvdDataReadWait path)

- Callsite (MP4): after allocating the read buffer, MP4 calls `DCInvalidateRange(buf, OSRoundUp32B(len))` before issuing `DVDReadAsync`.
  Evidence: `decomp_mario_party_4/src/game/dvd.c`
- Current port status: record-only; does not model cache effects, only tracks last `(addr,len)` for deterministic testing.
  Evidence: `src/sdk_port/os/OSCache.c`
- Testcase:
  - `dc_invalidate_range_mp4_hu_dvd_data_read_wait_001`: records `addr=0x80400000` and `len=0x40`.
    Evidence: `tests/sdk/os/dc_invalidate_range/expected/dc_invalidate_range_mp4_hu_dvd_data_read_wait_001.bin` and `tests/sdk/os/dc_invalidate_range/actual/dc_invalidate_range_mp4_hu_dvd_data_read_wait_001.bin`

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
