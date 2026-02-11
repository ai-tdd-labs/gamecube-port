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
| **PBT suites passing** | 13 (OSAlloc, MTX+Quat, ARQ, CARD-FAT, DVDFS, OSThread+Mutex+Msg, OSStopwatch, OSTime, PADClamp, dvdqueue, OSAlarm, GXTexture) |

### Per-Module Breakdown

| Module | Decomp | Ported | % | PBT Status | Notes |
|--------|--------|--------|---|------------|-------|
| **OSAlloc** | 12 | 16+ | ~100% | 2000/2000 PASS | Extra DL helpers; `-m32` struct match |
| **OSArena** | 6 | 6 | 100% | — | Fully complete |
| **OSThread** | 17 | 19 | ~100% | 726k/726k PASS | Scheduler + mutex + priority inheritance + JoinThread + Message + WaitCond + invariants |
| **OSMutex** | 11 | 11 | 100% | (covered by OSThread L4-L6, L10-L11) | Full: Lock/Unlock/TryLock/WaitCond/SignalCond/CheckDeadLock/CheckMutex |
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
| **OSAlarm** | 5 | 5 | 100% | 531k/531k PASS | Sorted DL insert/cancel/fire + periodic re-insert |
| **MTX** | 76 | 46 | 61% | PASS | mtx(23), vec(12), quat(8), mtx44(3) |
| **GX** | 261 | 123 | 47% | Integration | Largest module; smoke-test coverage |
| **VI** | 19 | 15 | 79% | — | |
| **SI** | 23 | 13 | 57% | — | |
| **PAD** | 15 | 9 | 60% | PADClamp 1.6M PASS | ClampStick/ClampTrigger/PADClamp |
| **DVD** | 66 | 20 | 30% | Built | DVDFS + dvdqueue PBT passing |
| **AR/ARQ** | 26 | 5 | 19% | Built | ARQ property tests passing |
| **CARD** | 69 | 4 | 6% | Built | CARD-FAT property tests passing |
| **EXI** | 23 | 0 | 0% | — | Not started |
| **DSP** | 19 | 0 | 0% | — | Not started |
| **AI** | 27 | 0 | 0% | — | Not started |
| **PPCArch** | 45 | 0 | 0% | — | Not started (mostly PPC-specific) |
| **DB** | 8 | 0 | 0% | — | Not started |

### Completion Tiers

1. **Complete (90%+):** OSAlloc, OSArena, OSThread+Mutex, OSMessage, OSStopwatch, OSAlarm
2. **Well underway (50-80%):** MTX+Quat, VI, SI, PAD
3. **Partial (20-50%):** GX, DVD, OSInterrupt, OSError
4. **Minimal (<20%):** CARD, AR/ARQ, OSCache, OSRtc, OS core
5. **Not started (0%):** EXI, DSP, AI, PPCArch, DB, OSContext, OSMemory, OSLink

---

## Property-Based Test (PBT) Suites

All PBT suites use the oracle pattern: exact decomp copy with `oracle_` prefix
compared against the `sdk_port` implementation using `gc_mem` big-endian emulation.

| Suite | Location | Build Script | Seeds | Checks | Status |
|-------|----------|-------------|-------|--------|--------|
| **OSAlloc** | `tests/sdk/os/os_alloc/property/` | `tools/run_property_test.sh` | 2000 | ~60k | PASS |
| **OSThread+Mutex+Msg** | `tests/sdk/os/osthread/property/` | `tools/run_osthread_property_test.sh` | 2000 | ~726k | PASS |
| **MTX+Quat** | `tests/sdk/mtx/property/` | `tools/run_mtx_property_test.sh` | 2000 | 100k | PASS |
| **OSStopwatch** | `tests/sdk/os/stopwatch/property/` | `tools/run_stopwatch_property_test.sh` | 2000 | ~622k | PASS |
| **OSTime** | `tests/sdk/os/ostime/property/` | `tools/run_ostime_property_test.sh` | 2000 | ~506k | PASS |
| **PADClamp** | `tests/sdk/pad/property/` | `tools/run_padclamp_property_test.sh` | 2000 | ~1.6M | PASS |
| **dvdqueue** | `tests/sdk/dvd/property/` | `tools/run_dvdqueue_property_test.sh` | 2000 | ~300k | PASS |
| **OSAlarm** | `tests/sdk/os/osalarm/property/` | `tools/run_osalarm_property_test.sh` | 2000 | ~531k | PASS |
| **GXTexture** | `tests/sdk/gx/property/` | `tools/run_gxtexture_property_test.sh` | 2000 | ~1.3M | PASS |
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
| L10 | WaitCond/SignalCond | Release mutex → sleep on cond → re-lock with saved count |
| L11 | Mutex invariants | CheckMutex, CheckDeadLock, CheckMutexes after every random op |

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

1. **OS module gaps:** OSContext (stubbed), OSMemory, OSLink
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
4. **OSCond + invariant checkers** — DONE, integrated into OSThread suite as L10-L11
5. **OSTicksToCalendarTime** — DONE, standalone PBT suite (506k checks)
6. **PADClamp** (3 functions) — DONE, standalone PBT suite (1.6M checks)
7. **dvdqueue** (6 functions) — DONE, standalone PBT suite (300k checks)
8. **OSAlarm** (5 functions) — DONE, standalone PBT suite (531k checks)
9. **GXTexture** (GXGetTexBufferSize + GetImageTileCount) — DONE, standalone PBT suite (1.3M checks)

### Evaluated and Skipped (not suitable for PBT)

| Module | Reason |
|--------|--------|
| **OSFont** | ROM-dependent lookup tables + LZ77 decoder needs crafted input |
| **OSArena** | Trivial bump allocator (4 getters/setters + 2 aligned allocs) |
| **OSReset** | OSRegisterResetFunction is same sorted DL list pattern as OSAlarm |
| **CARDCheck** | __CARDCheckSum is 4-line u16 sum loop; __CARDCompareFileName is bounded strcmp |
| **GXLight** | Math depends on PPC `__frsqrte` intrinsic and `cosf()` |
| **psmtx.c / mtxvec.c** | PPC paired-single assembly (no C decomp available) |
| **OSMemory** | Hardware register config, BAT/MMU assembly |
| **OSLink** | ELF relocation, cache ops |
| **OSContext** | PPC register save/restore (not needed for port) |
| **AR** (not ARQ) | DMA hardware, bus probing |
| **OSSync** | 1 function, pure hardware exception vector install |
| **VI/SI/EXI/DSP/AI** | Hardware interface modules, need extensive mock layer |

### Remaining PBT Candidates (new, from full decomp scan)

| Candidate | Source | Type | Suitability | Notes |
|-----------|--------|------|-------------|-------|
| **GXProject** | GXTransform.c:7-35 | Pure math | **Strong** | 3D projection: modelview transform -> perspective/ortho -> viewport map. No globals, no hardware, no side effects. |
| **THPAudioDecode** | THPAudio.c:3-146 | Signal processing | **Moderate** | ADPCM decoder: predictor coefficients, scale, saturation, nibble extraction. Need to define THPAudioRecordHeader + generate random encoded frames. |
| **GXSetFog (core math)** | GXPixel.c:7-56 | Float encoding | **Moderate** | Mantissa normalization loop (B_mant/B_expn), A/B/C computation from 4 floats. Extract math from register packing. |
| **__GXCalculateVLim** | GXAttr.c:123-171 | Lookup table | **Moderate** | Vertex limit from VCD register fields. Pure given (vcdLo, vcdHi, vatA) as inputs. Needs GET_REG_FIELD macro. |
| **GXInitFogAdjTable** | GXPixel.c:108-138 | Math + sqrtf | **Weak** | 10-entry table from width + projection matrix. Straightforward sqrtf loop. |
| **GXSetIndTexMtx** | GXBump.c:42-100 | Fixed-point encode | **Weak** | Simple `(int)(1024*val) & 0x7FF` for 6 values + scale_exp split. Too simple for PBT. |

### PBT coverage expansion (ongoing)

- Mutation gate enforcement for all suites
- Retail-trace replay for hardware-sensitive behaviors
