# VISetPostRetraceCallback (MP4 realistic HuPadInit)

MP4 `HuPadInit` installs a post-retrace callback:
1. `int lvl = OSDisableInterrupts()`
2. `VISetPostRetraceCallback(PadReadVSync)`
3. `OSRestoreInterrupts(lvl)`

This suite verifies our deterministic `sdk_port` model for:
- storing the callback pointer in RAM-backed state
- returning the previous callback
- call-count bookkeeping

Oracle:
- Dolphin expected (`dol/mp4/...`) vs host actual (`host/...`) on the RAM dump at `0x80300000`.

