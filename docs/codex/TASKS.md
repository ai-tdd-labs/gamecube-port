# Tasks

This file is the session-to-session TODO list. Keep it short, concrete, and
evidence-based. Mark items DONE with a commit hash.

## Now (priority order)

1. RAM-backed SDK state infrastructure (big-endian in MEM1)
   - [ ] Add minimal state page helpers (`src/sdk_port/sdk_state.h`).
   - [ ] Migrate OS arena pointers to RAM-backed state (OSGet/SetArenaLo/Hi).
   - [ ] Reset state page in smoke harnesses for determinism.
   - [ ] Run: `mp4_init_chain_001` Dolphin+host dumps and compare window.

2. Migrate more SDK state out of host process globals (incremental)
   - [ ] OSAlloc: move `__OSCurrHeap` + alloc metadata into RAM-backed state.
   - [ ] VI: move `gc_vi_*` observable state into RAM-backed state.
   - [ ] Update smoke oracle from snapshot-only to larger/full MEM1 ranges as
         subsystems become RAM-backed.

3. MP4 chain progress (tests from real callsites)
   - [ ] Keep `docs/sdk/mp4/MP4_chain_all.csv` as the source of SDK call order.
   - [ ] For each SDK function encountered: add one test per *unique call
         variant* seen in MP4 (and later other games), not synthetic scenarios.

## Later

1. Real-game checkpoint dumps (secondary oracle)
   - [ ] Find MP4 RVZ/ISO location and record in docs.
   - [ ] Add Dolphin GDB breakpoint-based dump (or timed fallback with warning).

