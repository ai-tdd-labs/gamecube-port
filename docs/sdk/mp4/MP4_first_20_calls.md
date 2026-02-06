# MP4 First 20 Calls (What We Copy vs What We Unit-Test)

This page is a quick, concrete on-ramp for "how do we port MP4 into our runtime without guessing".

Rules recap:
- **MP4 game code**: treat as workload. We vendor/copy it and do not change game logic.
- **Nintendo SDK calls** (OS/DVD/VI/PAD/GX/etc): we reimplement under `src/sdk_port/` and validate via deterministic tests.

Sources:
- MP4 `main()` and `HuSysInit()` call order:
  - `src/game_workload/mp4/vendor/src/game/main.c`
  - `src/game_workload/mp4/vendor/src/game/init.c`
- Canonical MP4 SDK coverage tracker:
  - `docs/sdk/mp4/MP4_chain_all.csv`

## Top-Level (MP4 `main()`)

| Order | Call | Origin | What we do |
|------:|------|--------|------------|
| 1 | `HuSysInit(...)` | MP4 | Copy as workload (already vendored). |
| 2 | `HuPrcInit()` | MP4 | Copy as workload. |
| 3 | `HuPadInit()` | MP4 | Copy as workload. |
| 4 | `GWInit()` | MP4 | Copy as workload. |
| 5 | `pfInit()` | MP4 | Copy as workload. |
| 6 | `HuSprInit()` | MP4 | Copy as workload. |
| 7 | `Hu3DInit()` | MP4 | Copy as workload. |
| 8 | `HuDataInit()` | MP4 | Copy as workload. |
| 9 | `HuPerfInit()` | MP4 | Copy as workload. |
| 10 | `HuPerfCreate(...)` | MP4 | Copy as workload. |
| 11 | `HuPerfCreate(...)` | MP4 | Copy as workload. |
| 12 | `WipeInit(RenderMode)` | MP4 | Copy as workload. |
| 13 | `omMasterInit(...)` | MP4 | Copy as workload. |
| 14 | `VIWaitForRetrace()` | SDK (VI) | Unit-test + port. Covered in `docs/sdk/mp4/MP4_chain_all.csv`. |
| 15 | `VIGetNextField()` | SDK (VI) | Unit-test + port. Covered in `docs/sdk/mp4/MP4_chain_all.csv`. |
| 16 | `OSReport(...)` | SDK (OS) | Unit-test + port. Covered in `docs/sdk/mp4/MP4_chain_all.csv`. |
| 17 | `VIWaitForRetrace()` | SDK (VI) | Same as (14). |
| 18 | loop begins (`VIGetRetraceCount()`, etc) | mixed | SDK calls are unit-tested/ported; MP4 calls are workload. |

Note:
- The **first non-MP4 work** (for runtime parity) is already at `VIWaitForRetrace()` in `main()`.
- MP4 "init" is mostly game code, but it *drives* many SDK calls inside `HuSysInit()`.

## Inside MP4 `HuSysInit()` (first 20 SDK calls)

This is the start of the "SDK chain" we port. These are **SDK** calls (not MP4 code).

All of these are tracked (and currently covered) in `docs/sdk/mp4/MP4_chain_all.csv` as `HuSysInit` rows 1..20:

| Order | SDK function |
|------:|--------------|
| 1 | `OSInit()` |
| 2 | `DVDInit()` |
| 3 | `VIInit()` |
| 4 | `PADInit()` |
| 5 | `OSGetProgressiveMode()` |
| 6 | `VIGetDTVStatus()` |
| 7 | `VIConfigure(...)` |
| 8 | `VIConfigurePan(...)` |
| 9 | `OSAlloc(...)` |
| 10 | `GXInit(...)` |
| 11 | `OSInitFastCast()` |
| 12 | `VIGetTvFormat()` |
| 13 | `OSPanic(...)` |
| 14 | `GXAdjustForOverscan(...)` |
| 15 | `GXSetViewport(...)` |
| 16 | `GXSetScissor(...)` |
| 17 | `GXSetDispCopySrc(...)` |
| 18 | `GXSetDispCopyDst(...)` |
| 19 | `GXSetDispCopyYScale(...)` |
| 20 | `OSReport(...)` |

What comes next (still inside `HuSysInit()`):
- More GX/OS/VI calls (framebuffer arena setup, heap init, VI flushing, viewport jitter, invalidate caches, etc).
- Those are already enumerated in `docs/sdk/mp4/MP4_chain_all.csv` (`HuSysInit` rows 21..55).

