# Tasks

This file is the session-to-session TODO list. Keep it short, concrete, and
evidence-based. Mark items DONE with a commit hash.

## Now (priority order)

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
   - [x] Fix+cover `OSDisableInterrupts/OSRestoreInterrupts` MP4-realistic test (DOL expected + host actual). (uncommitted)
   - [ ] Next MP4 init blocker: HuDataInit (DVDConvertPathToEntrynum / OSPanic on missing paths).

## Later

1. Real-game checkpoint dumps (secondary oracle)
   - [x] Find MP4 RVZ/ISO location and record in docs. (MP4_assets.md; hash recorded)
   - [x] Pick a real RVZ checkpoint address (symbol) and dump RAM at that point (PC polling). (d1654a3)
