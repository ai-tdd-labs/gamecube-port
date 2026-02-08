# GXSetCurrentMtx (MP4 Hu3DExec)

MP4 `Hu3DExec()` starts with:
- `GXSetCurrentMtx(0)`

This suite validates the observable state from the Dolphin SDK implementation:
- `matIdxA` low 6 bits updated
- XF reg 24 updated to `matIdxA`
- `bpSentNot` set to 1

Source of truth:
- `decomp_mario_party_4/src/dolphin/gx/GXTransform.c` (`GXSetCurrentMtx` + `__GXSetMatrixIndex`).
