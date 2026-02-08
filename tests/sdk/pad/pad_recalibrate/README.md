# PADRecalibrate (synthetic, MP4-style)

MP4 does not call `PADRecalibrate` in the early init chain, but other games and later code may.

This suite verifies our deterministic `sdk_port` model for `PADRecalibrate`:
- stores the mask into RAM-backed state (`GC_SDK_OFF_PAD_RECALIBRATE_MASK`)
- increments `GC_SDK_OFF_PAD_RECALIBRATE_CALLS`

Oracle:
- Dolphin expected (`dol/mp4/...`) vs host actual (`host/...`) on the RAM dump at `0x80300000`.
