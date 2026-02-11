# SDK Port Coverage — Mario Party 4

Last updated: 2026-02-11

## Summary

The game calls **~305 unique SDK functions** across 13 modules.
The port currently implements **~205 functions** (plus internal helpers).

| Module | Game needs | Ported | Coverage | Notes |
|--------|-----------|--------|----------|-------|
| **OS** | 48 | 42 | **88%** | Thread/Mutex/Msg via `port_` prefix; some macros (OSTicksToMilliseconds) |
| **GX** | 121 | 93 | **77%** | Largest module; missing indirect tex, some vertex formats |
| **DVD** | 12 | 11 | **92%** | Only DVDCancel missing |
| **PAD** | 8 | 7 | **88%** | Only PADButtonDown missing (likely macro) |
| **VI** | 13 | 12 | **92%** | Only VISetPreRetraceCallback missing |
| **SI** | 1 | 1 | **100%** | SISetSamplingRate |
| **MTX** | 33 | 33 | **100%** | All C_MTX* implemented; MTX*/PSMTX* are macros/aliases |
| **CARD** | 23 | 4 | **17%** | Only FAT internals; need full mount/open/read/write/close chain |
| **AR** | 10 | 0 | **0%** | ARAM memory manager — not started |
| **ARQ** | 2 | 2 | **100%** | ARQInit + ARQPostRequest (+ internal helpers) |
| **AI** | 7 | 0 | **0%** | Audio interface — not started |
| **THP** | 27 | 0 | **0%** | Video player — not started |
| **TOTAL** | **~305** | **~205** | **~67%** | |

---

## Per-Module Detail

### OS (42/48 = 88%)

**Ported (in sdk_port or as port_ functions):**
- OSAlloc, OSAllocFixed, OSAllocFromHeap, OSFree, OSFreeToHeap
- OSCreateHeap, OSDestroyHeap, OSAddToHeap, OSSetCurrentHeap, OSCheckHeap, OSDumpHeap, OSInitAlloc
- OSGetArenaHi, OSGetArenaLo, OSSetArenaHi, OSSetArenaLo
- OSInit, OSGetConsoleType, OSGetConsoleSimulatedMemSize, OSGetPhysicalMemSize, OSGetProgressiveMode
- OSDisableInterrupts, OSEnableInterrupts, OSRestoreInterrupts
- OSGetTime, OSReport, OSPanic, OSRoundUp32B, OSRoundDown32B, OSInitFastCast
- OSInitStopwatch, OSResetStopwatch, OSStartStopwatch, OSStopStopwatch, OSCheckStopwatch
- port_OSCreateThread, port_OSResumeThread, port_OSSuspendThread, port_OSCancelThread
- port_OSSleepThread, port_OSWakeupThread, port_OSYieldThread, port_OSExitThread, port_OSJoinThread
- port_OSInitMutex, port_OSLockMutex, port_OSUnlockMutex, port_OSTryLockMutex
- port_OSInitCond, port_OSWaitCond, port_OSSignalCond
- port_OSInitMessageQueue, port_OSSendMessage, port_OSReceiveMessage, port_OSJamMessage

**Missing (game needs but not ported):**
- `OSGetTick` — simple TBR read; need host equivalent (clock_gettime or similar)
- `OSTicksToCalendarTime` — PBT tested in oracle; not yet in sdk_port source
- `OSTicksToMilliseconds` — **macro** (`ticks / (OS_TIMER_CLOCK / 1000)`), just need the define
- `OSDumpStopwatch` — printf wrapper for stopwatch data, trivial
- `OSGetSoundMode` — SRAM read, need stub
- `OSGetResetButtonState` — hardware button, need stub
- `OSResetSystem` — system reset, need stub
- `OSSetIdleFunction` — callback setter, need stub
- `OSSetProgressiveMode` — SRAM write, need stub
- `OSLink` / `OSUnlink` — ELF module linker, complex (may stub)

### GX (93/121 = 77%)

**Ported (93 functions in GX.c):**
All core GX functions for rendering pipeline: Begin/End, vertex formats, TEV stages,
color/alpha blending, texture objects, projection, viewport, scissor, copy, lights, etc.

**Missing (28 functions):**
- Indirect texturing: `GXSetIndTexCoordScale`, `GXSetIndTexMtx`, `GXSetIndTexOrder`, `GXSetNumIndStages`, `GXSetTevDirect`, `GXSetTevIndTile`, `GXSetTevIndWarp`
- TEV Konstant: `GXSetTevKAlphaSel`, `GXSetTevKColor`, `GXSetTevKColorSel`, `GXSetTevColorS10`
- TEV Swap: `GXSetTevSwapMode`, `GXSetTevSwapModeTable`
- Vertex formats: `GXColor1x16`, `GXColor4u8`, `GXNormal1x16`, `GXNormal3s16`, `GXTexCoord1x16`, `GXTexCoord2s16`
- Texture: `GXInitTexObjCI`, `GXInitTlutObj`, `GXLoadTlut`, `GXSetTexCoordScaleManually`
- Misc: `GXGetProjectionv`, `GXPixModeSync`, `GXProject`, `GXResetWriteGatherPipe`, `GXNtsc480Prog`

### DVD (11/12 = 92%)

**Ported:** DVDInit, DVDGetDriveStatus, DVDConvertPathToEntrynum, DVDFastOpen, DVDOpen,
DVDClose, DVDRead, DVDReadAsync, DVDReadAsyncPrio, DVDReadPrio, DVDGetCommandBlockStatus

**Missing:** `DVDCancel` — cancel in-flight read operation

### PAD (7/8 = 88%)

**Ported:** PADInit, PADRead, PADClamp, PADReset, PADRecalibrate, PADSetSpec, PADControlMotor

**Missing:** `PADButtonDown` — likely a macro (`(status->button & button) != 0`)

### VI (12/13 = 92%)

**Ported:** VIInit, VIConfigure, VIConfigurePan, VIFlush, VIGetDTVStatus, VIGetNextField,
VIGetRetraceCount, VIGetTvFormat, VISetBlack, VISetNextFrameBuffer, VISetPostRetraceCallback,
VIWaitForRetrace

**Missing:** `VISetPreRetraceCallback` — symmetric to VISetPostRetraceCallback

### MTX (33/33 = 100%)

All MTX functions ported via C_MTX* implementations.
`MTX*` and `PSMTX*` are macros that alias to `C_MTX*` on non-PPC.

Includes: Identity, Copy, Concat, Inverse, InvXpose, Transpose, MultVec, MultVecSR,
RotRad, RotAxisRad, Scale, ScaleApply, Trans, TransApply, Quat, LookAt, Perspective,
Ortho, Frustum, LightPerspective, LightOrtho, LightFrustum, Reflect, DegToRad.

Plus VEC (12): Add, Subtract, Scale, Normalize, Mag, SquareMag, DotProduct, CrossProduct,
Distance, SquareDistance, HalfAngle, Reflect.

Plus QUAT (8): Add, Multiply, Normalize, Inverse, Slerp, RotAxisRad, Mtx.

**Still need for game:** `PSMTXMultVecArray`, `PSMTXReorder`, `PSMTXROMultVecArray`
(batch/reorder ops — PPC paired-single ASM, need C loop equivalents)

### CARD (4/23 = 17%)

**Ported:** CARDCheckSum, CARDUpdateFatBlock, CARDAllocBlock, CARDFreeBlock (internal FAT ops)

**Missing (19 API functions):**
CARDInit, CARDMount, CARDUnmount, CARDOpen, CARDClose, CARDCreate, CARDDelete,
CARDRead, CARDWrite, CARDFormat, CARDCheck, CARDFreeBlocks, CARDGetSectorSize,
CARDProbeEx, CARDGetSerialNo, CARDGetStatus, CARDSetStatus, CARDSetBannerFormat,
CARDSetCommentAddress, CARDSetIconAddress, CARDSetIconAnim, CARDSetIconFormat, CARDSetIconSpeed

Note: CARD needs EXI subsystem for actual hardware I/O. For port, we'll simulate
with host filesystem (similar to Dolphin emulator approach).

### AR (0/10 = 0%) — NOT STARTED

**Missing (10 functions used by game via `armem.c`, `kerent.c`, `msmsys.c`):**
- `ARInit` — initialize ARAM stack allocator + interrupt handler (hardware: `__DSPRegs`, `__ARChecksize`)
- `ARCheckInit` — return `__AR_init_flag` (trivial)
- `ARAlloc` — stack-push allocation from ARAM address space
- `ARFree` — stack-pop deallocation
- `ARGetSize` — return `__AR_Size`
- `ARSetSize` — empty stub in decomp
- `ARGetBaseAddress` — return `0x4000` constant
- `ARGetDMAStatus` — read `__DSPRegs[5]` (hardware)
- `ARRegisterDMACallback` — set/get callback pointer
- `ARStartDMA` — program DMA registers for ARAM↔MRAM transfer (hardware)

Note: ARAlloc/ARFree are pure stack-pointer math (PBT-suitable).
ARInit/ARStartDMA/ARGetDMAStatus are hardware-coupled (need stubs for port).
For port, ARAM can be simulated as a host malloc'd buffer with ARStartDMA doing memcpy.

### ARQ (2/2 = 100%)

**Ported:** `port_ARQInit`, `port_ARQPostRequest` (+ internal helpers: PopTaskQueueHi, ServiceQueueLo, InterruptServiceRoutine)

All game-needed ARQ functions are implemented. PBT suite passes (2000 seeds).

### AI (0/7 = 0%) — NOT STARTED

**Missing:** AIGetDMAStartAddr, AIInitDMA, AIRegisterDMACallback, AISetStreamPlayState,
AISetStreamVolLeft, AISetStreamVolRight, AIStartDMA

Note: Audio interface. For port, needs audio backend (SDL_audio or similar).

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

## PBT Coverage (17 suites — all pure-computation functions covered)

| Suite | Checks | Status |
|-------|--------|--------|
| OSAlloc | ~60k | PASS |
| OSThread+Mutex+Msg | ~726k | PASS |
| MTX+Quat | ~100k | PASS |
| OSStopwatch | ~622k | PASS |
| OSTime | ~506k | PASS |
| PADClamp | ~1.6M | PASS |
| dvdqueue | ~300k | PASS |
| OSAlarm | ~531k | PASS |
| GXTexture | ~1.3M | PASS |
| GXProject | ~1.6M | PASS |
| GXCompressZ16 | ~141M | PASS |
| THPAudioDecode | ~40M | PASS |
| GXGetYScaleFactor | ~4.3M | PASS |
| ARQ | — | PASS |
| CARD-FAT | — | PASS |
| DVDFS | — | PASS |

**Total:** ~200M+ checks across all suites, all passing (2000 seeds each).

---

## Priority for MP4 Boot

### Tier 1 — Required for basic init + main loop
1. OS stubs: OSGetTick, OSTicksToMilliseconds (macro), OSGetSoundMode, OSResetSystem
2. AR subsystem (10 functions) — ARAM init + allocator used in `armem.c` early boot
3. GX indirect texturing (7 functions) — used heavily in MP4 board rendering
4. GX TEV konstant colors (4 functions) — used in multi-texture effects
5. GX TEV swap modes (2 functions) — used in alpha/color channel routing
6. Missing GX vertex formats (6 functions) — GXColor4u8, normals, etc.

### Tier 2 — Required for full game
7. CARD subsystem (19 functions) — save/load game data
8. AI subsystem (7 functions) — audio playback
9. THP subsystem (27 functions) — cutscene video playback
10. Remaining GX functions (GXProject, GXGetProjectionv, etc.)

### Tier 3 — Nice to have
11. PSMTXMultVecArray/Reorder (batch ops)
12. DVDCancel, PADButtonDown, VISetPreRetraceCallback (minor)
13. OSLink/OSUnlink (ELF module loading — may not be needed for port)
