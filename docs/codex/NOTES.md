# Codex Notes (Facts Only)

Codex has no memory. The repository is the memory.

Rules:
- Facts only. No guesses.
- Every bullet must cite evidence (decomp path + symbol, or a test id + expected.bin).
- Separate: API contract vs internal implementation vs side effects.

## Confirmed Behaviors

### VISetNextFrameBuffer
- Callsites enumerated across MP4/TP/WW/AC.
  Evidence: docs/sdk/vi/VISetNextFrameBuffer.md
- Current DOL tests in this repo are scaffolding only (marker/control-flow).
  They do not yet validate real VI side effects (shadow regs / HorVer / HW regs).
  Evidence: tests/sdk/vi/vi_set_next_frame_buffer/dol/*; tests/sdk/vi/vi_set_next_frame_buffer/expected/vi_set_next_frame_buffer_{min,realistic_mp4,edge_unaligned}_001.bin

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
