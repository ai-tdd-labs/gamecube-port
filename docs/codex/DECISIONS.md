# Decisions

This file documents decisions we made and why.

## Legacy test migration policy

Decision:
- Do not blindly copy legacy DOL tests into `tests/sdk/`.
- Only treat a legacy test as a Dolphin oracle if the DOL executes real SDK behavior, i.e. compiled from decomp SDK sources under `decomp_mario_party_4/src/dolphin/...` (or other decomp roots).

Why:
- Several legacy tests implement "synthetic" versions of SDK functions inside the test file (mocked regs, mocked SRAM, stubbed interrupts).
- Those tests are useful as algorithm sketches, but they do not validate against Dolphin's real behavior and therefore do not satisfy "never guess".

What to do instead:
- When legacy is synthetic, rewrite the DOL test to call the decomp SDK function and dump results, then port the behavior under `src/sdk_port/` and compare expected vs actual.

## MP4 host workload harness headers (no decomp includes)

Decision:
- For the MP4 integration workload (`HuSysInit` reachability), compile decompiled MP4 code on host using **minimal, host-safe headers** under `tests/workload/include/` instead of including `decomp_mario_party_4/include`.

Why:
- The decomp include tree contains stubs like `stdint.h` (only `uintptr_t`) and PPC-only `asm { ... }` blocks (e.g. `OSFastCast.h`) which break host compilation and pollute libc prototypes.
- The workload harness is for **reachability + missing surface discovery**, not for correctness; correctness remains enforced by the deterministic DOL-vs-host unit tests.

Implementation:
- Use `tools/run_host_scenario.sh` with subsystem `workload`:
  - Adds `-I tests/workload/include` and compiles `src/game_workload/mp4/vendor/src/game/init.c`.
  - Links against `src/sdk_port/*` and stubs game-only externs in the scenario TU.
- First workload scenario: `tests/workload/mp4/mp4_husysinit_001_scenario.c` (expects marker `MP40/DEADBEEF` in `tests/actual/workload/mp4_husysinit_001.bin`).
