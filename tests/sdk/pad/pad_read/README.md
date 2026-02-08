# PADRead (MP4 realistic PadReadVSync)

MP4 `PadReadVSync` does:
- `PADRead(status)`
- `PADClamp(status)`

This suite verifies our deterministic `sdk_port` model for `PADRead`:
- increments the RAM-backed `GC_SDK_OFF_PAD_READ_CALLS`
- writes a deterministic default status (`err = PAD_ERR_NOT_READY`) so MP4 init code can run without real controllers

Oracle:
- Dolphin expected (`dol/mp4/...`) vs host actual (`host/...`) on the RAM dump at `0x80300000`.
