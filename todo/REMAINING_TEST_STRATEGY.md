# Remaining Functions — Test Strategy

Last updated: 2026-02-14

## Context

The game needs **~305 SDK functions**. Currently **~213 are ported**.
The remaining **~85 functions** are all hardware-coupled — every pure-computation
function is already covered by PBT (18 suites, ~250M+ checks).

MP4 workload reachability ladder (host, regression only):
- `tools/run_mp4_workload_ladder.sh` currently reaches:
  - `mp4_mainloop_thousand_iter_tick_001` (1000 VI ticks)
  - `mp4_process_*` scheduler scenarios
  - MTX variants + wipe scenarios (`mp4_wipe_frame_still_mtx_001`, `mp4_wipe_crossfade_mtx_001`)
  - pfDrawFonts opt-in minimal draw step (`mp4_mainloop_one_iter_tick_pf_draw_001`)
- Knobs:
  - `GC_HOST_WORKLOAD_MTX=1` links MTX into selected workload steps.
  - `GC_HOST_WORKLOAD_WIPE_CROSSFADE=1` enables WipeCrossFade texture-copy subset (scenario-specific).
  - `GC_HOST_WORKLOAD_PF_DRAW=1` enables a tiny opt-in quad draw inside the pfDrawFonts host slice (scenario-specific).

Method migration queue (current):
- `VISetBlack` has been migrated to unified L0-L5 DOL-PBT (`tools/run_vi_set_black_pbt.sh`).
- PAD family is already migrated to unified L0-L5 DOL-PBT with mutation checks, and is the template for remaining upgrades.
- `GXSetTevSwapMode` has been migrated to unified L0-L5 DOL-PBT (`tools/run_gx_set_tev_swap_mode_pbt.sh`).
- `GXSetTevKColor` / `GXSetTevKAlphaSel` have been migrated to unified L0-L5 DOL-PBT (`tools/run_gx_set_tev_kcolor_pbt.sh`).
- `GXSetTevColorS10` has been migrated to unified L0-L5 DOL-PBT (`tools/run_gx_set_tev_color_s10_pbt.sh`).
- `GXSetTevIndTile` has been migrated to unified L0-L5 DOL-PBT (`tools/run_gx_set_tev_ind_tile_pbt.sh`).
- `GXColor1x16` has been migrated to unified L0-L5 DOL-PBT (`tools/run_gx_color_1x16_pbt.sh`).
- `GXColor4u8` has been migrated to unified L0-L5 DOL-PBT (`tools/run_gx_color_4u8_pbt.sh`).
- `GXNormal1x16` has been migrated to unified L0-L5 DOL-PBT (`tools/run_gx_normal_1x16_pbt.sh`).
- `GXNormal3s16` has been migrated to unified L0-L5 DOL-PBT (`tools/run_gx_normal_3s16_pbt.sh`).
- `GXTexCoord1x16` has been migrated to unified L0-L5 DOL-PBT (`tools/run_gx_tex_coord_1x16_pbt.sh`).
- `GXTexCoord2s16` has been migrated to unified L0-L5 DOL-PBT (`tools/run_gx_tex_coord_2s16_pbt.sh`).
- `GXInitTexObjCI` has been migrated to unified L0-L5 DOL-PBT (`tools/run_gx_init_tex_obj_ci_pbt.sh`).
- `GXInitTlutObj` has been migrated to unified L0-L5 DOL-PBT (`tools/run_gx_init_tlut_obj_pbt.sh`).
- `GXLoadTlut` has been migrated to unified L0-L5 DOL-PBT (`tools/run_gx_load_tlut_pbt.sh`).
- `GXSetTexCoordScaleManually` has been migrated to unified L0-L5 DOL-PBT (`tools/run_gx_set_tex_coord_scale_manually_pbt.sh`).
- `GXSetTevSwapModeTable` has been migrated to unified L0-L5 DOL-PBT (`tools/run_gx_set_tev_swap_mode_table_pbt.sh`).
- AR hardware layer has been migrated to unified L0-L5 DOL-PBT:
  - `ARStartDMA` (`tools/run_ar_start_dma_pbt.sh`)
  - `ARGetDMAStatus` (`tools/run_ar_get_dma_status_pbt.sh`)
  - `ARRegisterDMACallback` (`tools/run_ar_register_dma_callback_pbt.sh`)
  - `ARSetSize` (`tools/run_ar_set_size_pbt.sh`)

## Remaining test workload snapshot

| Bucket | Remaining |
|--------|-----------|
| Trace replay | **~71** |
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

## Remaining ~84 functions by test method

### Trace replay needed (~79 functions)

These are hardware-coupled and can only be verified against Dolphin ground truth.

| Module | Count | Functions | Complexity |
|--------|-------|-----------|------------|
| **GX** | 0 | — | Remaining GX blocker is now data compatibility: `GXNtsc480Prog` weak symbol wiring |
| **CARD** | 0 | — | Setter helpers are macros/inlines on `CARDStat` (not SDK functions); no remaining trace-replay-needed CARD API in current list |
| **THP** | 27 | `THPInit`, `THPVideoDecode`, `THPSimpleOpen/Close/Decode/PreLoad/LoadStop/Init/Quit`, `THPSimpleSetBuffer/SetVolume/CalcNeedMemory`, `THPSimpleAudioStart/Stop`, `THPSimpleGetTotalFrame/GetVideoInfo/DrawCurrentFrame`, `THPGXYuv2RgbSetup/Draw`, `THPGXRestore`, `THPAudioMixCallback`, `THPDecodeFunc`, `THPViewFunc/ViewSprFunc`, `THPTestProc`, `THPSimpleInlineFunc` | Large — JPEG codec + ADPCM + locked cache emulation |
| **AI** | 0 | — | Medium — needs audio backend (SDL_audio or similar) |

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

## New work: trace-guided constrained-random (DOL-PBT hybrid)

Goal: use harvested retail traces as input/state constraints, then run seeded
high-volume randomized cases in Dolphin (oracle) and host (`sdk_port`) with
bit-exact comparison.

This is **not** a replacement for trace replay; it extends it for wider coverage.

### Directory split (important)

- `tests/trace-harvest/` = raw captured retail traces (ground-truth samples).
- `tests/trace-guided/` = derived models + generated case batches (new).
- `tests/sdk/<module>/<function>/...` = executable DOL/host test suites that
  consume those artifacts.

### Implementation tasks

1. Add trace model schema (`model.json`) per target function:
   - valid ranges, enums, pointer/struct constraints, call-order rules.
2. Add constrained generator:
   - 70% near-trace mutation
   - 20% boundary values
   - 10% exploratory within constraints
3. Add deterministic case format (`cases.bin` / `cases.jsonl`) with seed + case_id.
4. Add DOL batch runner:
   - executes N cases, records compact outputs/state hashes.
5. Add host batch runner:
   - replays same N cases against `sdk_port`.
6. Add diff tool:
   - first mismatch by case_id + offset + seed reproduction.
7. Add mutation gate for each new trace-guided suite (`tools/mutations/*`).
8. Start with one pilot function, then template-rollout.

### Pilot recommendation

- First pilot: `OSTicksToCalendarTime` or `PSMTXConcat` (low side effects).
- Second pilot: one hardware-near callback/register function (e.g. VI/PAD).

---

## Priority order

1. **GX gaps** (17) — blocking MP4 board rendering (indirect tex, TEV konstant/swap, vertex formats)
2. **AR hardware** (4) — ARStartDMA needed for ARAM data loading
3. **CARD** (19) — save/load game data
4. **AI** (7) — audio playback
5. **THP** (27) — cutscene video
6. **MTX batch** (3) — PBT, not urgent
7. **Trivial stubs** (~10) — port when needed, no tests required
