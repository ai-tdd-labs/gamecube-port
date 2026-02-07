# OSDisableInterrupts / OSRestoreInterrupts (MP4 realistic)

This test covers the nested critical-section pattern used by MP4 (HuPadInit and many SDK subsystems):

1. `OSDisableInterrupts()` (outer)
2. `OSDisableInterrupts()` (inner)
3. `OSRestoreInterrupts(inner_level)`
4. `OSRestoreInterrupts(outer_level)`

Oracle:
- Dolphin expected (`dol/mp4/...`) vs host actual (`host/...`) on the RAM dump at `0x80300000`.

