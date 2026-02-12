# Remaining Functions — Test Strategy

Last updated: 2026-02-12 (pm)

## Context

The game needs **~305 SDK functions**. Currently **~208 are ported**.
The remaining **~90 functions** are all hardware-coupled — every pure-computation
function is already covered by PBT (18 suites, ~250M+ checks).

## Remaining test workload snapshot

| Bucket | Remaining |
|--------|-----------|
| Trace replay | **~76** |
| PBT | **~0** |
| No test needed | **~10** |

### Test types in this project

| Type | What | When to use |
|------|------|-------------|
| **PBT** (Property-Based Testing) | Oracle (exact decomp copy with `oracle_` prefix) vs port, random seeds, bit-exact comparison | Pure computation only (same input → same output, no hardware side effects) |
| **Trace replay** | DOL runs in Dolphin → `expected.bin`, host port → `actual.bin`, diff = pass/fail | Hardware-coupled functions — Dolphin provides ground truth |
| **Workload integration** | Real MP4 decomp code compiled on host, runs init chains | Crash detection only, no correctness oracle |

PBT and trace replay are the only methods that provide **correctness proof**.
Workloads catch crashes but don't verify output correctness.

**Important:** There are no "self-invented" unit tests in this project. All test
inputs come from either random fuzzing (PBT) or real decomp callsites verified
against Dolphin. This avoids circular reasoning (testing decomp interpretation
against decomp interpretation).

---

## Remaining ~90 functions by test method

### Trace replay needed (~80 functions)

These are hardware-coupled and can only be verified against Dolphin ground truth.

| Module | Count | Functions | Complexity |
|--------|-------|-----------|------------|
| **GX** | 22 | Indirect tex (6): `GXSetIndTexCoordScale`, `GXSetIndTexMtx`, `GXSetIndTexOrder`, `GXSetTevDirect`, `GXSetTevIndTile`, `GXSetTevIndWarp`. TEV konstant (4): `GXSetTevKAlphaSel`, `GXSetTevKColor`, `GXSetTevKColorSel`, `GXSetTevColorS10`. TEV swap (2): `GXSetTevSwapMode`, `GXSetTevSwapModeTable`. Vertex formats (6): `GXColor1x16`, `GXColor4u8`, `GXNormal1x16`, `GXNormal3s16`, `GXTexCoord1x16`, `GXTexCoord2s16`. Texture (4): `GXInitTexObjCI`, `GXInitTlutObj`, `GXLoadTlut`, `GXSetTexCoordScaleManually` | Medium — register packing, SET_REG_FIELD macros |
| **CARD** | 19 | `CARDInit`, `CARDMount`, `CARDUnmount`, `CARDOpen`, `CARDClose`, `CARDCreate`, `CARDDelete`, `CARDRead`, `CARDWrite`, `CARDFormat`, `CARDCheck`, `CARDFreeBlocks`, `CARDGetSectorSize`, `CARDProbeEx`, `CARDGetSerialNo`, `CARDGetStatus`, `CARDSetStatus`, `CARDSetBannerFormat`, `CARDSetCommentAddress`+ icon setters | Large — needs EXI simulation, host filesystem backend |
| **THP** | 27 | `THPInit`, `THPVideoDecode`, `THPSimpleOpen/Close/Decode/PreLoad/LoadStop/Init/Quit`, `THPSimpleSetBuffer/SetVolume/CalcNeedMemory`, `THPSimpleAudioStart/Stop`, `THPSimpleGetTotalFrame/GetVideoInfo/DrawCurrentFrame`, `THPGXYuv2RgbSetup/Draw`, `THPGXRestore`, `THPAudioMixCallback`, `THPDecodeFunc`, `THPViewFunc/ViewSprFunc`, `THPTestProc`, `THPSimpleInlineFunc` | Large — JPEG codec + ADPCM + locked cache emulation |
| **AI** | 7 | `AIGetDMAStartAddr`, `AIInitDMA`, `AIRegisterDMACallback`, `AISetStreamPlayState`, `AISetStreamVolLeft`, `AISetStreamVolRight`, `AIStartDMA` | Medium — needs audio backend (SDL_audio or similar) |
| **AR** | 4 | `ARStartDMA` (DMA = memcpy simulation), `ARGetDMAStatus` (hardware reg read), `ARRegisterDMACallback` (callback setter), `ARSetSize` (empty stub in decomp) | Small — ARStartDMA is just memcpy between host ARAM buffer and MEM1 |
| **DVD** | 1 | `DVDCancel` — cancel in-flight read | Small |

### PBT possible (~0 functions)

No remaining pure-computation SDK gaps currently require new PBT.

### No test needed (~10 functions)

Trivial stubs, macros, or one-liners where testing adds no value.

| Module | Count | Functions | Why no test needed |
|--------|-------|-----------|--------------------|
| **OS** | 5 | `OSGetTick` (host clock read), `OSDumpStopwatch` (printf wrapper), `OSGetSoundMode` (return constant), `OSSetIdleFunction` (callback setter), `OSSetProgressiveMode` (SRAM write stub) | 1-3 lines each, no logic to verify |
| **OS** | 2 | `OSTicksToMilliseconds` (macro), `OSGetResetButtonState` (return 0) | Single-expression macros/stubs |
| **OS** | 1 | `OSResetSystem` (stub — no-op or exit()) | No meaningful behavior to test |
| **PAD** | 1 | `PADButtonDown` — likely macro `(status->button & button) != 0` | Single expression |
| **OS** | 2 | `OSLink` / `OSUnlink` — ELF module linker | May not be needed for port at all |

---

## Trace replay infrastructure

Each trace replay test has two sides:

```
tests/sdk/<module>/<function>/
  dol/mp4/<scenario>/        ← PPC code, runs in Dolphin emulator
    <scenario>.c             ← Calls real SDK function, writes results to 0x80300000
  host/
    <scenario>_scenario.c    ← Calls sdk_port function, writes results to gc_mem
```

**Flow:**
1. Compile DOL-side → run in Dolphin → dump RAM region → `expected/<scenario>.bin`
2. Compile host-side → run on host → dump gc_mem → `actual/<scenario>.bin`
3. `diff expected.bin actual.bin` → PASS if identical

**Runner:** `tools/run_host_scenario.sh <scenario.c>` (set `GC_SCENARIO_COMPARE=1` for comparison)

**Current count:** 216 trace replay scenarios (210 per-function + 6 smoke chains)
across GX (109), OS (50), VI (20), PAD (14), DVD (11), SI (4), MTX (2).

---

## Priority order

1. **GX gaps** (22) — blocking MP4 board rendering (indirect tex, TEV konstant/swap, vertex formats)
2. **AR hardware** (4) — ARStartDMA needed for ARAM data loading
3. **CARD** (19) — save/load game data
4. **AI** (7) — audio playback
5. **THP** (27) — cutscene video
6. **MTX batch** (3) — PBT, not urgent
7. **Trivial stubs** (~10) — port when needed, no tests required
