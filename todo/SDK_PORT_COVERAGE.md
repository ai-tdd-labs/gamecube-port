# SDK Port Coverage — Mario Party 4

Last updated: 2026-02-13

## Summary

The game calls **~305 unique SDK functions** across 13 modules.
The port currently implements **~233 functions** (plus internal helpers).

Current migration status:
- PAD function suites are now in unified L0-L5 DOL-PBT format (`PADControlMotor`, `PADSetSpec`, `PADReset`, `PADInit`, `PADRead`, `PADClamp`, `PADRecalibrate`).
- VI now includes a unified L0-L5 DOL-PBT suite for `VISetBlack` (`tools/run_vi_set_black_pbt.sh`, mutation-checked with `tools/mutations/vi_set_black_flip_test.patch`).
- GX `GXSetTevSwapMode` now includes unified L0-L5 DOL-PBT coverage (`tools/run_gx_set_tev_swap_mode_pbt.sh`, mutation-checked with `tools/mutations/gx_set_tev_swap_mode_flip.patch`).
- GX `GXSetTevKColor` / `GXSetTevKAlphaSel` now include unified L0-L5 DOL-PBT coverage (`tools/run_gx_set_tev_kcolor_pbt.sh`, mutation-checked with `tools/mutations/gx_set_tev_kalpha_sel_shift.patch`).
- GX `GXSetTevColorS10` now includes unified L0-L5 DOL-PBT coverage (`tools/run_gx_set_tev_color_s10_pbt.sh`, mutation-checked with `tools/mutations/gx_set_tev_color_s10_wrong_g_shift.patch`).
- GX `GXSetTevIndTile` now includes unified L0-L5 DOL-PBT coverage (`tools/run_gx_set_tev_ind_tile_pbt.sh`, mutation-checked with `tools/mutations/gx_set_tev_ind_tile_wrong_wrap.patch`).
- GX `GXColor1x16` now includes unified L0-L5 DOL-PBT coverage (`tools/run_gx_color_1x16_pbt.sh`, mutation-checked with `tools/mutations/gx_color_1x16_truncate.patch`).
- GX `GXColor4u8` now includes unified L0-L5 DOL-PBT coverage (`tools/run_gx_color_4u8_pbt.sh`, mutation-checked with `tools/mutations/gx_color_4u8_swap_rg.patch`).

## Remaining test workload snapshot

| Bucket | Remaining |
|--------|-----------|
| Trace replay | **~71** |
| PBT | **~0** |
| No test needed | **~10** |

| Module | Game needs | Ported | Coverage | Notes |
|--------|-----------|--------|----------|-------|
| **OS** | 48 | 47 | **98%** | Thread/Mutex/Msg; +OSTicksToCalendarTime, OSGetTick, OSDumpStopwatch, OSAlarm |
| **GX** | 121 | 105 | **87%** | +GXProject, GXCompressZ16, GXDecompressZ16, GXPixModeSync, GXGetProjectionv, GXResetWriteGatherPipe, GXSetNumIndStages, GXSetIndTexCoordScale, GXSetIndTexOrder, GXSetTevDirect, GXSetIndTexMtx, GXSetTevIndWarp |
| **DVD** | 12 | 12 | **100%** | +DVDCancel, dvdqueue port in sdk_port |
| **PAD** | 8 | 8 | **100%** | +PADClamp |
| **VI** | 13 | 13 | **100%** | VI callback setter pair covered |
| **SI** | 1 | 1 | **100%** | SISetSamplingRate |
| **MTX** | 33 | 33 | **100%** | Includes PSMTXReorder, PSMTXROMultVecArray, PSMTXMultVecArray |
| **CARD** | 23 | 14 | **61%** | FAT internals + Dir ops + Unlock crypto (exnor, bitrev, CARDRand) |
| **AR** | 10 | 6 | **60%** | ARInit, ARAlloc, ARFree, ARCheckInit, ARGetBaseAddress, ARGetSize |
| **ARQ** | 2 | 2 | **100%** | ARQInit + ARQPostRequest (+ internal helpers) |
| **AI** | 7 | 0 | **0%** | Audio interface — not started |
| **THP** | 27 | 0 | **0%** | Video player — not started |
| **TOTAL** | **~305** | **~234** | **~76%** | |

---

## Per-Module Detail

### OS (47/48 = 98%)

**Ported (in sdk_port or as port_ functions):**
- OSAlloc, OSAllocFixed, OSAllocFromHeap, OSFree, OSFreeToHeap
- OSCreateHeap, OSDestroyHeap, OSAddToHeap, OSSetCurrentHeap, OSCheckHeap, OSDumpHeap, OSInitAlloc
- OSGetArenaHi, OSGetArenaLo, OSSetArenaHi, OSSetArenaLo
- OSInit, OSGetConsoleType, OSGetConsoleSimulatedMemSize, OSGetPhysicalMemSize, OSGetProgressiveMode
- OSDisableInterrupts, OSEnableInterrupts, OSRestoreInterrupts
- OSGetTime, OSGetTick, OSReport, OSPanic, OSRoundUp32B, OSRoundDown32B, OSInitFastCast
- OSTicksToCalendarTime
- OSInitStopwatch, OSResetStopwatch, OSStartStopwatch, OSStopStopwatch, OSCheckStopwatch, OSDumpStopwatch
- OSCreateAlarm, OSSetAlarm, OSSetPeriodicAlarm, OSCancelAlarm (+ InsertAlarm, FireHead)
- port_OSCreateThread, port_OSResumeThread, port_OSSuspendThread, port_OSCancelThread
- port_OSSleepThread, port_OSWakeupThread, port_OSYieldThread, port_OSExitThread, port_OSJoinThread
- port_OSInitMutex, port_OSLockMutex, port_OSUnlockMutex, port_OSTryLockMutex
- port_OSInitCond, port_OSWaitCond, port_OSSignalCond
- port_OSInitMessageQueue, port_OSSendMessage, port_OSReceiveMessage, port_OSJamMessage

**Missing (1 function):**
- `OSTicksToMilliseconds` — **macro** (`ticks / (OS_TIMER_CLOCK / 1000)`), just need the define

### GX (105/121 = 87%)

**Ported (93 functions in GX.c):**
All core GX functions for rendering pipeline: Begin/End, vertex formats, TEV stages,
color/alpha blending, texture objects, projection, viewport, scissor, copy, lights, etc.

**Missing (16 functions):**
- Indirect texturing: `GXSetTevIndTile`
- TEV Konstant: `GXSetTevKAlphaSel`, `GXSetTevKColor`, `GXSetTevKColorSel`, `GXSetTevColorS10`
- TEV Swap: `GXSetTevSwapMode`, `GXSetTevSwapModeTable`
- Vertex formats: `GXColor1x16`, `GXColor4u8`, `GXNormal1x16`, `GXNormal3s16`, `GXTexCoord1x16`, `GXTexCoord2s16`
- Texture: `GXInitTexObjCI`, `GXInitTlutObj`, `GXLoadTlut`, `GXSetTexCoordScaleManually`
- Misc: `GXNtsc480Prog`

### DVD (12/12 = 100%)

**Ported:** DVDInit, DVDGetDriveStatus, DVDConvertPathToEntrynum, DVDFastOpen, DVDOpen,
DVDClose, DVDRead, DVDReadAsync, DVDReadAsyncPrio, DVDReadPrio, DVDGetCommandBlockStatus

**Missing:** none in current MP4-needed DVD set.

### PAD (7/8 = 88%)

**Ported:** PADInit, PADRead, PADClamp, PADReset, PADRecalibrate, PADSetSpec, PADControlMotor

**Missing:** `PADButtonDown` — likely a macro (`(status->button & button) != 0`)

### VI (13/13 = 100%)

**Ported:** VIInit, VIConfigure, VIConfigurePan, VIFlush, VIGetDTVStatus, VIGetNextField,
VIGetRetraceCount, VIGetTvFormat, VISetBlack, VISetNextFrameBuffer, VISetPostRetraceCallback,
VIWaitForRetrace

**Missing:** none.

### MTX (33/33 = 100%)

All MTX functions ported via C_MTX* implementations.
`MTX*` and `PSMTX*` are macros that alias to `C_MTX*` on non-PPC.

Includes: Identity, Copy, Concat, Inverse, InvXpose, Transpose, MultVec, MultVecSR,
RotRad, RotAxisRad, Scale, ScaleApply, Trans, TransApply, Quat, LookAt, Perspective,
Ortho, Frustum, LightPerspective, LightOrtho, LightFrustum, Reflect, DegToRad.

Plus VEC (12): Add, Subtract, Scale, Normalize, Mag, SquareMag, DotProduct, CrossProduct,
Distance, SquareDistance, HalfAngle, Reflect.

Plus QUAT (8): Add, Multiply, Normalize, Inverse, Slerp, RotAxisRad, Mtx.

**Still need for game:** none in current MTX batch set.
(batch/reorder ops — PPC paired-single ASM, need C loop equivalents)

### CARD (14/23 = 61%)

**Ported:**
- FAT internals: `__CARDCheckSum`, `__CARDUpdateFatBlock`, `__CARDAllocBlock`, `__CARDFreeBlock`
- Directory ops: `__CARDCompareFileName`, `__CARDAccess`, `__CARDIsPublic`, `__CARDGetFileNo`
- Seek: `__CARDSeek` (FAT chain traversal)
- Unlock crypto: `exnor_1st`, `exnor`, `bitrev`, `CARDSrand`, `CARDRand`

PBT suites: CARD-FAT (AllocBlock/FreeBlock/CheckSum) + CARD-Dir (CompareFileName/Access/IsPublic/GetFileNo/Seek) + CARD-Unlock (exnor_1st/exnor/bitrev/CARDRand) — 330k+ checks, all PASS.

**Missing (9 API functions):**
CARDInit, CARDMount, CARDUnmount, CARDOpen, CARDClose, CARDCreate, CARDDelete,
CARDRead, CARDWrite, CARDFormat, CARDCheck, CARDFreeBlocks, CARDGetSectorSize,
CARDProbeEx, CARDGetSerialNo, CARDGetStatus, CARDSetStatus, CARDSetBannerFormat,
CARDSetCommentAddress, CARDSetIconAddress, CARDSetIconAnim, CARDSetIconFormat, CARDSetIconSpeed

Note: Remaining CARD functions need EXI subsystem for actual hardware I/O.
For port, we'll simulate with host filesystem (similar to Dolphin emulator approach).

### AR (6/10 = 60%)

**Ported:**
- `ARInit` — stack allocator init (hardware stripped, `__ARChecksize` elided)
- `ARAlloc` — stack-push allocation from ARAM address space
- `ARFree` — stack-pop deallocation
- `ARCheckInit` — return init flag
- `ARGetBaseAddress` — return `0x4000` constant
- `ARGetSize` — return ARAM size

PBT suite: 344,753 checks PASS (2000 seeds).

**Missing (4 hardware functions):**
- `ARSetSize` — empty stub in decomp
- `ARGetDMAStatus` — read `__DSPRegs[5]` (hardware)
- `ARRegisterDMACallback` — set/get callback pointer
- `ARStartDMA` — program DMA registers for ARAM↔MRAM transfer (hardware)

### ARQ (2/2 = 100%)

**Ported:** `port_ARQInit`, `port_ARQPostRequest` (+ internal helpers: PopTaskQueueHi, ServiceQueueLo, InterruptServiceRoutine)

All game-needed ARQ functions are implemented. PBT suite passes (2000 seeds).

### AI (0/7 = 0%) — HARDWARE ONLY

**Missing:** AIGetDMAStartAddr, AIInitDMA, AIRegisterDMACallback, AISetStreamPlayState,
AISetStreamVolLeft, AISetStreamVolRight, AIStartDMA

Note: All 21 functions in ai.c are pure hardware register I/O (`__AIRegs`, `__DSPRegs`).
No pure computation suitable for PBT. For port, needs audio backend (SDL_audio or similar).

### THP (0/27 = 0%) — NOT STARTED

**Missing:** THPInit, THPVideoDecode, THPAudioDecode, THPSimpleOpen, THPSimpleClose,
THPSimpleDecode, THPSimplePreLoad, THPSimpleLoadStop, THPSimpleInit, THPSimpleQuit,
THPSimpleSetBuffer, THPSimpleSetVolume, THPSimpleCalcNeedMemory,
THPSimpleAudioStart, THPSimpleAudioStop, THPSimpleGetTotalFrame, THPSimpleGetVideoInfo,
THPSimpleDrawCurrentFrame, THPGXYuv2RgbSetup, THPGXYuv2RgbDraw, THPGXRestore,
THPAudioMixCallback, THPDecodeFunc, THPViewFunc, THPViewSprFunc, THPTestProc,
THPSimpleInlineFunc

Note: THP = GameCube video format (JPEG frames + ADPCM audio).
THPAudioDecode is PBT-tested but not in port source yet. THPVideoDecode uses
Locked Cache + paired-single (needs host JPEG decoder).

---

## PBT Coverage (23 suites — all pure-computation functions covered)

| Suite | Checks | Status | Links to sdk_port |
|-------|--------|--------|-------------------|
| OSAlloc | ~60k | PASS | yes |
| OSThread+Mutex+Msg | ~726k | PASS | yes |
| MTX+Quat | ~100k | PASS | yes |
| OSStopwatch | ~622k | PASS | yes |
| OSTime | ~506k | PASS | yes (OSTime.c) |
| PADClamp | ~1.6M | PASS | yes (PAD.c) |
| dvdqueue | ~300k | PASS | yes (dvdqueue.c) |
| OSAlarm | ~531k | PASS | yes (OSAlarm.c) |
| GXTexture | ~1.3M | PASS | yes (GX.c) |
| GXProject | ~1.6M | PASS | yes (GX.c) |
| GXCompressZ16 | ~141M | PASS | yes (GX.c) |
| THPAudioDecode | ~40M | PASS | inline (no sdk_port yet) |
| GXGetYScaleFactor | ~4.3M | PASS | yes (GX.c) |
| AR | ~345k | PASS | yes (ar.c) |
| ARQ | — | PASS | yes (arq.c) |
| CARD-FAT | — | PASS | yes |
| CARD-Dir | ~78k | PASS | yes (card_dir.c) |
| CARD-Unlock | ~252k | PASS | yes (card_unlock.c) |
| GXLight | ~60k | PASS | yes (GX.c) |
| MTX44 | ~20k | PASS | yes (mtx44.c) |
| GXOverscan | ~16k | PASS | yes (GX.c) |
| DVDFS | — | PASS | yes |

**Total:** ~200M+ checks across all suites, all passing (2000 seeds each).
All oracle functions verified as exact copies of mp4-decomp Nintendo SDK code (141 functions, 0 mismatches).

### PBT Coverage Analysis

All pure-computation functions in the decomp have been PBT-tested. Remaining untested modules:
- **AI**: 21 functions — all hardware register I/O, no PBT possible
- **AR DMA**: ARStartDMA, ARGetDMAStatus, ARRegisterDMACallback — all hardware, no PBT possible
- **THP video**: THPDec.c has ~14 pure functions (Huffman tables, IDCT, MCU decompression) but they use PPC paired-single ASM and locked cache — would need host JPEG decoder reimplementation
- **CARD remaining**: 14 API functions need EXI subsystem mock for hardware I/O

---

## Priority for MP4 Boot

### Tier 1 — Required for basic init + main loop
1. ~~OS stubs: OSGetTick~~ DONE — OSTicksToMilliseconds (macro) still needed
2. ~~AR allocator~~ 6/10 DONE — remaining 4 are hardware stubs (ARStartDMA, ARGetDMAStatus, ARRegisterDMACallback, ARSetSize)
3. GX indirect texturing (7 functions) — used heavily in MP4 board rendering
4. GX TEV konstant colors (4 functions) — used in multi-texture effects
5. GX TEV swap modes (2 functions) — used in alpha/color channel routing
6. Missing GX vertex formats (6 functions) — GXColor4u8, normals, etc.

### Tier 2 — Required for full game
7. CARD subsystem (19 functions) — save/load game data
8. AI subsystem (7 functions) — audio playback
9. THP subsystem (27 functions) — cutscene video playback
10. Remaining GX misc functions (`GXNtsc480Prog`)

### Tier 3 — Nice to have
11. MTX batch ops (covered)
12. VI callback pair (covered)
13. OSLink/OSUnlink (ELF module loading — may not be needed for port)
