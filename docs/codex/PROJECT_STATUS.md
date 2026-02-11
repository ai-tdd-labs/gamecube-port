# Gamecube SDK Port — Project Status

Last updated: 2026-02-11

## Overview

Port of the Nintendo GameCube SDK (Dolphin OS) used by Mario Party 4, from PPC
big-endian to a host-side C implementation with `gc_mem` big-endian emulation.

**Decomp source:** `external/mp4-decomp/src/dolphin/`
**Port source:** `src/sdk_port/`
**Tests:** `tests/sdk/*/property/`, `tests/pbt/`

---

## Porting Progress

### Summary

| Metric | Value |
|--------|-------|
| **Decomp functions available** | ~854 |
| **Port functions implemented** | ~296 |
| **Overall completion** | ~35% |
| **PBT suites passing** | 9 (OSAlloc, MTX+Quat, ARQ, CARD-FAT, DVDFS, OSThread+Mutex+Message, OSStopwatch) |

### Per-Module Breakdown

| Module | Decomp | Ported | % | PBT Status | Notes |
|--------|--------|--------|---|------------|-------|
| **OSAlloc** | 12 | 16+ | ~100% | 2000/2000 PASS | Extra DL helpers; `-m32` struct match |
| **OSArena** | 6 | 6 | 100% | — | Fully complete |
| **OSThread** | 17 | 19 | ~100% | 518k/518k PASS | Scheduler + mutex + priority inheritance + JoinThread + Message |
| **OSMutex** | 11 | 7 | ~64% | (covered by OSThread L4-L6) | Init/Lock/Unlock/TryLock/InitCond/SignalCond; WaitCond/CheckDeadLock not yet |
| **OSMessage** | 4 | 4 | 100% | (covered by OSThread L7-L9) | Init/Send/Receive/Jam; circular buffer FIFO+LIFO |
| **OSStopwatch** | 6 | 6 | 100% | 622k/622k PASS | All 6 functions ported + PBT |
| **OSCache** | 23 | 3 | 13% | — | Minimal (DCInvalidateRange etc.) |
| **OSError** | 5 | 2 | 40% | — | |
| **OSInterrupt** | 12 | 3 | 25% | — | Disable/Restore/Enable stubs |
| **OSRtc** | 19 | 3 | 16% | — | |
| **OS (core)** | 25 | 3 | 12% | — | OSInit, OSGetConsoleType stubs |
| **OSContext** | 13 | 0 | 0% | — | Not needed for port (no real PPC context) |
| **OSMemory** | 7 | 0 | 0% | — | |
| **OSLink** | 9 | 0 | 0% | — | |
| **OSAlarm** | 5 | 0 | 0% | — | |
| **MTX** | 76 | 46 | 61% | PASS | mtx(23), vec(12), quat(8), mtx44(3) |
| **GX** | 261 | 123 | 47% | Integration | Largest module; smoke-test coverage |
| **VI** | 19 | 15 | 79% | — | |
| **SI** | 23 | 13 | 57% | — | |
| **PAD** | 15 | 9 | 60% | — | |
| **DVD** | 66 | 20 | 30% | Built | DVDFS property tests passing |
| **AR/ARQ** | 26 | 5 | 19% | Built | ARQ property tests passing |
| **CARD** | 69 | 4 | 6% | Built | CARD-FAT property tests passing |
| **EXI** | 23 | 0 | 0% | — | Not started |
| **DSP** | 19 | 0 | 0% | — | Not started |
| **AI** | 27 | 0 | 0% | — | Not started |
| **PPCArch** | 45 | 0 | 0% | — | Not started (mostly PPC-specific) |
| **DB** | 8 | 0 | 0% | — | Not started |

### Completion Tiers

1. **Complete (90%+):** OSAlloc, OSArena, OSThread+Mutex, OSMessage, OSStopwatch
2. **Well underway (50-80%):** MTX+Quat, VI, SI, PAD
3. **Partial (20-50%):** GX, DVD, OSInterrupt, OSError
4. **Minimal (<20%):** CARD, AR/ARQ, OSCache, OSRtc, OS core
5. **Not started (0%):** EXI, DSP, AI, PPCArch, DB, OSContext, OSMemory, OSLink, OSAlarm

---

## Property-Based Test (PBT) Suites

All PBT suites use the oracle pattern: exact decomp copy with `oracle_` prefix
compared against the `sdk_port` implementation using `gc_mem` big-endian emulation.

| Suite | Location | Build Script | Seeds | Checks | Status |
|-------|----------|-------------|-------|--------|--------|
| **OSAlloc** | `tests/sdk/os/os_alloc/property/` | `tools/run_property_test.sh` | 2000 | ~60k | PASS |
| **OSThread+Mutex+Message** | `tests/sdk/os/osthread/property/` | `tools/run_osthread_property_test.sh` | 2000 | ~518k | PASS |
| **MTX+Quat** | `tests/sdk/mtx/property/` | `tools/run_mtx_property_test.sh` | 2000 | 100k | PASS |
| **OSStopwatch** | `tests/sdk/os/stopwatch/property/` | `tools/run_stopwatch_property_test.sh` | 2000 | ~622k | PASS |
| **ARQ** | `tests/sdk/ar/property/` | `tools/run_arq_property_test.sh` | — | — | PASS |
| **CARD-FAT** | `tests/sdk/card/property/` | `tools/run_card_fat_property_test.sh` | — | — | PASS |
| **DVDFS** | `tests/sdk/dvd/dvdfs/property/` | `tools/run_dvdfs_property_test.sh` | — | — | PASS |

### OSThread+Mutex+Message Test Levels

| Level | Name | What it tests |
|-------|------|---------------|
| L0 | Basic lifecycle | Create, resume, scheduling |
| L1 | Suspend/Resume | Priority preemption cycles |
| L2 | Sleep/Wakeup | Wait queues, yield |
| L3 | Random integration | All ops including mutex Lock/TryLock/Unlock |
| L4 | Mutex basic | Lock/Unlock/TryLock random mix |
| L5 | Priority inheritance | Contended lock promotes owner, restored on unlock |
| L6 | JoinThread | Exit → MORIBUND → Join collects val |
| L7 | Message basic | Non-blocking Send/Receive across multiple queues |
| L8 | Message jam | LIFO Jam + FIFO Send wraparound on small queues |
| L9 | Message + threads | Message ops with thread switching, yield, resume |

### Additional PBT Suites (core)

| Suite | Location | Notes |
|-------|----------|-------|
| MTX core | `tests/pbt/mtx/mtx_core_pbt.c` | Strict leaf oracle (C_MTXIdentity, C_MTXOrtho) |
| DVD core | `tests/pbt/dvd/dvd_core_pbt.c` | Strict leaf oracle (read-window semantics) |
| OS round | `tests/pbt/os/os_round_32b/` | Strict leaf (OSRoundUp32B, OSRoundDown32B) |

---

## Architecture

### Key Design Principles

- **Evidence-first:** All behavior backed by decomp, symbols, or Dolphin runtime state
- **Oracle pattern:** Exact decomp copy with `oracle_` prefix — never hacked semantics
- **Big-endian emulation:** `gc_mem` layer maps GC addresses to host memory with byte-swap
- **Deterministic tests:** Same seed → same result; bit-exact oracle vs port comparison
- **Modular porting:** Each SDK subsystem ported independently with per-module PBT

### Key Files

| Path | Purpose |
|------|---------|
| `src/sdk_port/gc_mem.c` | GC memory mapper (big-endian emulation) |
| `src/sdk_port/sdk_state.h` | RAM-backed SDK global state |
| `tests/harness/gc_host_ram.h` | Test harness for GC RAM simulation |
| `tools/run_pbt_chain_gate.sh` | One-button gate for all PBT suites |
| `docs/codex/PBT_CHAIN_PROGRAM.md` | Definition of Done for PBT coverage |

### Build Environment

- **Platform:** Windows/MSYS2 (also builds on macOS)
- **Compiler:** `clang` (LLVM); `cc` not available on Windows
- **Flags:** `-D_XOPEN_SOURCE=700 -Wno-implicit-function-declaration`
- **Note:** `--gc-sections` linker warning on MSVC (harmless)

---

## Remaining Work (high-level)

### To reach MP4 boot

1. **OS module gaps:** OSContext (stubbed), OSMemory, OSLink, OSAlarm, OSMessage
2. **EXI:** Required for memory card and serial I/O — 23 functions, not started
3. **DSP:** Audio subsystem — 19 functions, not started
4. **AI:** Audio interface — 27 functions, not started
5. **CARD:** Only 6% done; needs full mount/read/write/format chain
6. **GX:** 47% done but largest module (261 functions); integration-tested
7. **DVD:** 30% done; async read chain partially covered

### Recently Completed PBT Chains

1. **OSMessage** (4 functions) — DONE, integrated into OSThread suite as L7-L9
2. **Quaternion math** (3 new functions) — DONE, integrated into MTX suite
3. **OSStopwatch** (6 functions) — DONE, standalone PBT suite

### Next PBT Candidates

- **WaitCond/CheckDeadLock** — remaining OSMutex functions
- **VI/SI/PAD** — hardware interface modules (need mock layer)
- **CARD** full chain — mount/read/write/format

### PBT coverage expansion (ongoing)

- OSThread+Mutex: WaitCond, CheckDeadLock (not yet tested)
- Add PBT for VI, SI, PAD (currently no property tests)
- Mutation gate enforcement for all suites
- Retail-trace replay for hardware-sensitive behaviors

### Modules NOT suitable for PBT

- **OSAlarm** — exception handlers, hardware timer decrementer
- **OSMemory** — hardware register config, BAT/MMU assembly
- **OSLink** — ELF relocation, cache ops
- **AR** (not ARQ) — DMA hardware, bus probing
- **OSSync** — 1 function, pure hardware exception vector install
