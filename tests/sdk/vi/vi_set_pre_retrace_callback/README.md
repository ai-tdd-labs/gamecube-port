# VISetPreRetraceCallback (deterministic callback swap)

This suite mirrors the same callback-swap pattern used in MP4 around retrace callbacks:
1. `int lvl = OSDisableInterrupts()`
2. `VISetPreRetraceCallback(PadReadVSync)`
3. `OSRestoreInterrupts(lvl)`

This suite verifies our deterministic `sdk_port` model for:
- storing the callback pointer in RAM-backed state
- returning the previous callback
- call-count bookkeeping

Oracle:
- Dolphin expected (`dol/mp4/...`) vs host actual (`host/...`) on the RAM dump at `0x80300000`.
