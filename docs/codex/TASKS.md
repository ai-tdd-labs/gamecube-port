# Tasks

This file is the session-to-session TODO list. Keep it short, concrete, and
evidence-based. Mark items DONE with a commit hash.

## Now (priority order)

MP4 boot-forward chain (bd):
- [x] HuPrcInit callsite inventory complete; no new SDK blocker identified.
  Evidence: `docs/sdk/mp4/HuPrcInit_blockers.md`
- [x] GWInit checkpoint scenario validation (host marker + RVZ probe alignment).
- [x] HuPrcInit->GWInit RVZ probe compare and delta note.
- [ ] Post-checkpoint gap analysis: auto-create next task batch from first unresolved blockers.
- [x] pfInit blocker inventory complete; no uncovered SDK calls in `printfunc.c`.
  Evidence: `docs/sdk/mp4/pfInit_blockers.md`
- [x] HuSprInit checkpoint scenario + RVZ probe alignment complete.
  Evidence: `tests/oracles/mp4_rvz/probes/husprinit_pc_8000D348_hit1/`
- [x] Hu3DInit blocker inventory complete; no new uncovered SDK calls in early path.
  Evidence: `docs/sdk/mp4/Hu3DInit_blockers.md`
- [x] HuPerfCreate inventory complete; no new SDK blockers.
  Evidence: `docs/sdk/mp4/HuPerfCreate_blockers.md`
- [x] WipeInit checkpoint scenario + RVZ probe alignment complete.
  Evidence: `tests/oracles/mp4_rvz/probes/wipeinit_pc_80041170_hit1/`
- [x] omMasterInit inventory complete; first unresolved SDK blockers identified.
  Evidence: `docs/sdk/mp4/omMasterInit_blockers.md`
- [ ] Test layout migration: move to explicit `tests/trace-harvest`, `tests/sdk/callsite`, `tests/sdk/pbt`.
  Evidence target: `docs/codex/TEST_LAYOUT_MIGRATION.md`
  Progress:
  - [x] Phase 2 complete: trace corpora moved to `tests/trace-harvest`; legacy `tests/traces` removed.
  - [x] Phase 3 complete: `tests/sdk/callsite` mirrors legacy SDK suite paths via symlinks.
  - [x] Phase 4 complete: `tests/sdk/pbt` mirrors legacy `tests/pbt` + sdk property suites via symlinks.
  - [x] Phase 5 complete: replay/harvest docs+scripts retargeted to `tests/trace-harvest`.
- [x] Small SDK blocker batch landed: `OSGetTick`, `OSTicksToCalendarTime`, `OSDumpStopwatch`, `DVDCancel`.
  Evidence: `src/sdk_port/os/OSRtc.c`, `src/sdk_port/os/OSStopwatch.c`, `src/sdk_port/dvd/DVD.c`,
  `tests/sdk/os/ostime/property/ostime_sdk_port_unit_test.c`, `tests/sdk/dvd/property/dvdcancel_unit_test.c`

Oracle exactness hardening (new):
- [ ] Add tier tag to each oracle (`STRICT_DECOMP`, `DECOMP_ADAPTED`, `MODEL_OR_SYNTHETIC`) and print in test output.
  Evidence target: `tests/sdk/*/property/*_oracle.h`, property runner scripts under `tools/`
- [ ] Build delta ledgers for current adapted oracles:
  - `ARQ`: interrupt + callback + DMA mocking deltas
  - `CARD FAT`: update-fat erase/write callback deltas
  - `OSAlloc`: struct/layout/assert host deltas
  - `DVDFS`: physical-mem mapping + path-rule deltas
  - `MTX`: asm removed / C fallback deltas
  Evidence target: `docs/sdk/*/ORACLE_DELTA.md`
- [ ] Add strict-vs-adapted dual-run checks where feasible (same fixtures, compare outputs).
  Evidence target: `tools/run_oracle_dualcheck.sh`
  Progress:
  - [x] Phase 1: strict dual-check added for CARD `__CARDCheckSum` (leaf) via `tools/run_oracle_dualcheck.sh`.
  - [ ] Phase 2: expand strict dual-checks to ARQ/OSAlloc/DVDFS/MTX where extraction is feasible.
    - [x] MTX strict leaf oracle (`C_MTXIdentity`, `C_MTXOrtho`) wired into `tests/pbt/mtx/mtx_core_pbt.c`.
    - [x] ARQ strict leaf extraction (callback normalization, decomp hack semantics) wired into `tests/sdk/ar/property/arq_property_test.c`.
    - [x] OSAlloc strict leaf extraction (`OSRoundUp32B`, `OSRoundDown32B`) wired into `tests/pbt/os/os_round_32b/os_round_32b_pbt.c`.
    - [x] DVDFS strict leaf extraction (`dvd_core` read-window semantics) wired into `tests/pbt/dvd/dvd_core_pbt.c`.
- [ ] Add retail-trace replay fixtures for hardware-sensitive behaviors (interrupt/callback ordering).
  Evidence target: `tests/oracles/mp4_rvz/*`, replay scripts in `tools/`
- [ ] Merge/reconcile `codex/integration-all` into `main` after gate passes.
  Evidence target: merge commit + green gate run

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
   - [x] Next MP4 init step: determine the *actual next SDK blocker after HuDataInit* by tracing from `HuDataDirRead` into `HuDataReadNumHeapShortForce` and `HuDvdDataReadWait`.
     Output:
       - new rows in `docs/sdk/mp4/MP4_chain_all.csv`
       - MP4-realistic suites under `tests/sdk/*`
       - facts in `docs/codex/NOTES.md`
     Evidence anchor: `decomp_mario_party_4/src/game/data.c` and `decomp_mario_party_4/src/game/dvd.c`
     Progress:
       - [x] Identified + covered `DCInvalidateRange` callsite used by `HuDvdDataReadWait`. (new suite `tests/sdk/os/dc_invalidate_range`)
       - [x] Covered the remaining HuDataInit DVD path calls (`DVDFastOpen`, `DVDReadAsync` poll + callback variant, `DVDGetCommandBlockStatus`) with deterministic virtual-disc backend. (c2f8f72, 2cc9c27, 8037773)
       - [x] Confirmed from decomp callflow that immediate post-`HuDataInit` blockers in this path are fully covered in chain rows; next meaningful risk moves to frame-loop and non-init DVD paths (e.g. THP `DVDReadPrio`/`DVDReadAsyncPrio`).

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

4. MP4 omMaster blockers (OSLink / OSUnlink)
  - [x] Harvest first retail `OSLink` case from RVZ (`0x800B7D24`) with module/global windows.
     Evidence: `tests/trace-harvest/os_link/mp4_rvz_v1/hit_000001_pc_800B7D24_lr_8003222C/`
  - [x] Build `OSLink` trace replay harness and pass harvested case(s) on host.
    Evidence:
      - `tools/replay_trace_case_os_link.sh`
      - `tests/sdk/os/os_link/host/os_link_rvz_trace_replay_001_scenario.c`
  - [ ] Harvest `OSUnlink` RVZ cases (`0x800B8180`) with same window layout.
    Current status: 0 hits in 60s + 240s boot-window runs.
    Target: `tests/trace-harvest/os_unlink/mp4_rvz_v1/`
    Prereq: reproducible later-game trigger path for objdll unload.
    Extra signal: `omOvlKill` (`0x8002F014`) also not hit in 180s boot-window probe.
   - [x] Build `OSUnlink` trace replay harness.
     Evidence:
      - `tools/replay_trace_case_os_unlink.sh`
      - `tests/sdk/os/os_unlink/host/os_unlink_rvz_trace_replay_001_scenario.c`
      - synthetic validation case: `tests/trace-harvest/os_unlink/synth_case_001` (marker `0xDEADBEEF`)
   - [ ] Pass harvested `OSUnlink` case(s) bit-exact on host.
     Current status: harvest script runs, but no real `OSUnlink` hits captured yet.
  - [ ] Re-run omMaster-adjacent checkpoint after both replay harnesses pass.

5. DVD path expansion (MP4)
   - [x] Add `DVDReadPrio` MP4 suite and verify bit-exact expected/actual pass.
     Evidence:
       - `tests/sdk/dvd/dvd_read_prio/dol/mp4/dvd_read_prio_mp4_init_mem_001/`
       - `tests/sdk/dvd/dvd_read_prio/host/dvd_read_prio_mp4_init_mem_001_scenario.c`
       - `tests/sdk/dvd/dvd_read_prio/expected/dvd_read_prio_mp4_init_mem_001.bin`
       - `tests/sdk/dvd/dvd_read_prio/actual/dvd_read_prio_mp4_init_mem_001.bin`
