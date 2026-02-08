# PADReset (MP4 realistic PadReadVSync)

MP4 `PadReadVSync` conditionally calls `PADReset(chan_mask)` when it detects certain controller errors.

This suite verifies our deterministic `sdk_port` model for `PADReset`:
- stores the last reset mask into RAM-backed state (`GC_SDK_OFF_PAD_RESET_MASK`)
- increments `GC_SDK_OFF_PAD_RESET_CALLS`

Oracle:
- Dolphin expected (`dol/mp4/...`) vs host actual (`host/...`) on the RAM dump at `0x80300000`.
