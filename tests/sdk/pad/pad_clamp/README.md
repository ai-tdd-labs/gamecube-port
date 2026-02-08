# PADClamp (MP4 realistic PadReadVSync)

MP4 `PadReadVSync` calls `PADClamp(status)` after `PADRead(status)`.

This suite verifies our deterministic `sdk_port` model for `PADClamp`:
- increments the RAM-backed `GC_SDK_OFF_PAD_CLAMP_CALLS`

Oracle:
- Dolphin expected (`dol/mp4/...`) vs host actual (`host/...`) on the RAM dump at `0x80300000`.
