# SISetSamplingRate (MP4 realistic HuPadInit)

MP4 `HuPadInit` calls `SISetSamplingRate(0)` after `PADInit()`.

This suite verifies our deterministic `sdk_port` model for:
- `SISetSamplingRate(msec)` clamping + stored sampling rate
- the derived `SISetXY(line,count)` parameters
- interrupt critical section bookkeeping (via `OSDisableInterrupts` / `OSRestoreInterrupts` model)

Oracle:
- Dolphin expected (`dol/mp4/...`) vs host actual (`host/...`) on the RAM dump at `0x80300000`.

