# Tasks

This file is the session-to-session TODO list. Keep it short, concrete, and
evidence-based. Mark items DONE with a commit hash.

## Now (priority order)

PBT chain program (whole-chain quality gate):
- [x] Define subsystem-level DoD and gating policy for OSAlloc/DVDFS/ARQ/CARD-FAT/MTX.
  Evidence: `docs/codex/PBT_CHAIN_PROGRAM.md`

PBT matrix baseline:
- [x] Add cross-subsystem matrix baseline (OSAlloc/DVDFS/ARQ/CARD-FAT/MTX).
  Evidence: `docs/sdk/PBT_CHAIN_MATRIX.md`
- [x] Add MTX core PBT suite and pass 50k iterations.
  Evidence: `tests/pbt/mtx/mtx_core_pbt.c`, `tools/run_pbt.sh mtx_core 50000 0xC0DEC0DE`
- [x] Add DVD core PBT suite and pass 20k iterations.
  Evidence: `tests/pbt/dvd/dvd_core_pbt.c`, `tools/run_pbt.sh dvd_core 20000 0xC0DEC0DE`
- [x] Import ARQ and CARD-FAT property suites on this branch and validate on macOS.
  Evidence: `tools/run_arq_property_test.sh --num-runs=50 -v`, `tools/run_card_fat_property_test.sh --num-runs=50 -v`
- [x] Add PBT oracle linkage map and one-button chain gate script.
  Evidence: `docs/codex/PBT_ORACLE_LINKAGE.md`, `tools/run_pbt_chain_gate.sh`

0. Real-game breakpoint dump tooling (secondary oracle)
   - [x] Large MEM1 dumps work reliably with conservative chunking (`--chunk 0x1000`) and reconnect/retry logic in `tools/ram_dump.py`.
     Evidence: `tools/dump_expected_mem1.sh`, `tests/sdk/smoke/mp4_pad_init_chain_001/expected/mp4_pad_init_chain_001_mem1.bin`
   - [x] Add RVZ MEM1 dump helper at PC checkpoint (`tools/dump_expected_rvz_mem1_at_pc.sh`).
     Note: now prefers Z0/Z1 breakpoint; falls back to PC polling if unsupported.
   - [x] Validate RVZ checkpoint dump on MP4: stop at `OSDisableInterrupts` PC `0x800B723C`, dump MEM1, archive under `tests/oracles/mp4_rvz/`.
     Evidence: `tests/oracles/mp4_rvz/mp4_rvz_pc_800B723C_mem1_24m.bin`

1. RAM-backed SDK state infrastructure (big-endian in MEM1)
   - [x] Add minimal state page helpers (`src/sdk_port/sdk_state.h`). (68ac0f7)
   - [x] Migrate OS arena pointers to RAM-backed state (OSGet/SetArenaLo/Hi). (68ac0f7)
   - [x] Reset state page in smoke harnesses for determinism. (68ac0f7, aa3b0c8)
   - [x] Run: `mp4_init_chain_001` Dolphin+host dumps and compare window. (verified)

2. Migrate more SDK state out of host process globals (incremental)
   - [x] OSAlloc: move `__OSCurrHeap` + alloc metadata into RAM-backed state. (17f3b6a)
   - [x] VI: move `gc_vi_*` observable state into RAM-backed state. (aa3b0c8)
   - [x] DVD/PAD/GX: migrate minimal observable state used by smoke snapshot. (aa3b0c8, 0706f15)
   - [x] Update smoke oracle to include sdk_state page in MEM1 compares. (aa3b0c8)

3. MP4 chain progress (tests from real callsites)
   - [x] Keep `docs/sdk/mp4/MP4_chain_all.csv` as the source of SDK call order. (7ddf625)
   - [ ] For each SDK function encountered: add one test per *unique call
         variant* seen in MP4 (and later other games), not synthetic scenarios.
   - [x] MP4 HuPadInit blockers: MP4 cases for `SISetSamplingRate`, `VISetPostRetraceCallback`, `PADControlMotor`, plus `PADSetSpec(PAD_SPEC_5)` and update smoke chain. (9ad732e)
   - [x] Extend MP4 chain with remaining GXInit tail calls after `GXSetDither` and add missing GX setters/pokes. (0706f15)
   - [x] Append HuPadInit SDK call order into `MP4_chain_all.csv` (PAD/SI/OS/VI sequence) and keep coverage in-sync. (cf9b190)
   - [x] Add HuPerfInit/HuPerfBegin SDK calls (OSStopwatch init + GX draw sync callback/token). (d7245e5)
   - [x] Fix+cover `OSDisableInterrupts/OSRestoreInterrupts` MP4-realistic test (DOL expected + host actual).
     Evidence: `tests/sdk/smoke/mp4_pad_init_chain_001/`
   - [x] HuDataInit: cover `DVDConvertPathToEntrynum` MP4 callsite behavior (minimal deterministic backend).
     Evidence: `tests/sdk/dvd/dvd_convert_path_to_entrynum/`
   - [x] HuDataDirRead path: cover `DVDFastOpen` + `DVDReadAsync` + `DVDGetCommandBlockStatus` MP4 callsite behavior (minimal deterministic backend). (8037773)
     Evidence: `tests/sdk/dvd/dvd_fast_open/` and `tests/sdk/dvd/dvd_read_async/`
   - [ ] Next MP4 init step: determine the *actual next SDK blocker after HuDataInit* by tracing from `HuDataDirRead` into `HuDataReadNumHeapShortForce` and `HuDvdDataReadWait`.
     Output:
       - new rows in `docs/sdk/mp4/MP4_chain_all.csv`
       - MP4-realistic suites under `tests/sdk/*`
       - facts in `docs/codex/NOTES.md`
     Evidence anchor: `decomp_mario_party_4/src/game/data.c` and `decomp_mario_party_4/src/game/dvd.c`
     Progress:
       - [x] Identified + covered `DCInvalidateRange` callsite used by `HuDvdDataReadWait`. (new suite `tests/sdk/os/dc_invalidate_range`)
       - [x] Covered the remaining HuDataInit DVD path calls (`DVDFastOpen`, `DVDReadAsync` poll + callback variant, `DVDGetCommandBlockStatus`) with deterministic virtual-disc backend. (c2f8f72, 2cc9c27, 8037773)

   - [ ] Next MP4 frame-loop SDK blockers: trace `Hu3DPreProc` + `Hu3DExec` (hsfman.c) and keep adding MP4-realistic GX callsite suites.
     Starting point:
       - [x] `GXSetCopyClear`, `GXSetCurrentMtx`, `GXSetDrawDone`, `GXWaitDrawDone` MP4 callsites. (this session)
       - [x] `GXSetFog` MP4 callsite (`Hu3DFogClear` style). (this session)
       - [x] `GXSetProjection` MP4 callsite (shadow-copy ortho). (this session)
     Next likely GX gaps inside `Hu3DExec` / shadow path:
       - `GXSetTexCopySrc`, `GXSetTexCopyDst`, `GXCopyTex`, `GXClearVtxDesc`, `GXSetVtxDesc`, `GXSetVtxAttrFmt`, `GXSetTevColor`, `GXLoadPosMtxImm`, `GXBegin`, `GXColor3u8`, ...

## Later

1. Real-game checkpoint dumps (secondary oracle)
   - [x] Find MP4 RVZ/ISO location and record in docs. (MP4_assets.md; hash recorded)
   - [x] Pick a real RVZ checkpoint address (symbol) and dump RAM at that point (PC polling). (d1654a3)
