# PADSetSpec (MP4 realistic)

MP4 `HuPadInit()` calls `PADSetSpec(PAD_SPEC_5)` before `PADInit()`.

This suite validates that `src/sdk_port/pad/PAD.c` persists:
- `Spec` (via `GC_SDK_OFF_PAD_SPEC`)
- a stable `MakeStatus` selection token (via `GC_SDK_OFF_PAD_MAKE_STATUS_KIND`)

Oracle: Dolphin expected.bin vs host actual.bin.
