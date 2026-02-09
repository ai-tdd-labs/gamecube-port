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

## PAD stubs for MP4 workload linking

Decision:
- For host workload builds that compile MP4 `src/game/pad.c`, provide a minimal PAD SDK surface in `src/sdk_port/pad/PAD.c` for linkability: `PADRead`, `PADClamp`, `PADReset`, `PADRecalibrate`.

Why:
- MP4 `pad.c` takes the address of `PadReadVSync` and therefore must link the full translation unit.
- Even if the workload does not trigger retrace callbacks, the linker still needs all undefined symbols referenced by `pad.c` to be defined somewhere.

Scope:
- These stubs are deterministic and only record minimal observable side effects into the RAM-backed sdk_state page.
- Correctness (real hardware semantics) must be established via dedicated DOL expected vs host actual unit tests for any behavior that becomes observable to the game.

## RVZ MEM1 dumps are not bit-comparable to host without a loader

Decision:
- Do not treat a full MEM1 dump (`0x80000000` size `0x01800000`) from retail MP4 RVZ as bit-comparable to a host workload MEM1 dump until we implement a real DOL loader / memory image on host.

Why:
- Retail RVZ runs the real MP4 DOL; MEM1 contains the game's loaded `.text/.data/.bss`, plus SDK globals at their real addresses.
- Our host workloads run selected MP4 C translation units and do **not** load the MP4 DOL image into virtual RAM, so large regions (especially code/data) are zero or different by construction.
- Result: the first mismatch is typically early in MEM1 (shortly after the ignored low-memory prefix) and does not indicate an SDK-port bug.

What to do instead (until loader exists):
- Use the deterministic oracle as primary: `sdk_port` on PPC (Dolphin DOL) vs `sdk_port` on host (virtual RAM).
- For RVZ sanity checks, probe **specific symbol addresses** (e.g. `__OSArenaLo`, `__OSArenaHi`, SI/VI state) and compare derived invariants, not the entire MEM1 image.

## MP4 vendor import: next files after initial boot slice

Decision:
- The next MP4 decomp translation units to import into `src/game_workload/mp4/vendor/` are:
  - `printfunc.c` (pfInit/pfClsScr/pfDrawFonts)
  - `wipe.c` (WipeInit/WipeExecAlways)
  - `objmain.c` (omMasterInit + overlay/object manager entrypoints)

Why:
- These functions are invoked during MP4 init + every mainloop iteration (`main.c`), so they are the first large MP4-specific “real code” that we currently stub in host workload scenarios.
- Importing these TUs increases *reachability* (we can run more real MP4 code on host) and surfaces the next missing SDK/game-only dependencies deterministically (via compile/link errors and runtime asserts).
- SDK correctness remains enforced separately via `tests/sdk/*` and PPC-vs-host smoke chains; this import is about “can we run further”, not “is it correct”.

Scope / guardrails:
- Import corresponding headers from `decomp_mario_party_4/include/game/` as-needed into `tests/workload/include/game/` (host-safe subset), not the full decomp include tree.
- If a TU drags in large subsystems (audio, overlays, etc.), keep it linkable by adding minimal game-only stubs in `tests/workload/mp4/slices/` until we deliberately expand that subsystem.
