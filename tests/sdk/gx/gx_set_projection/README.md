# GXSetProjection (MP4 Hu3DShadowExec Ortho)

MP4 `Hu3DShadowExec()` ends its shadow-copy path with:
- `C_MTXOrtho(sp18, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f);`
- `GXSetProjection(sp18, GX_ORTHOGRAPHIC);`

Evidence:
- `decomp_mario_party_4/src/game/hsfman.c` (`Hu3DShadowExec`, near the `GXCopyTex` block)
- SDK implementation: `decomp_mario_party_4/src/dolphin/gx/GXTransform.c:GXSetProjection`

This suite validates the *observable* projection packing and XF register writes:
- projType
- projMtx[0..5] packed from the input matrix
- XF regs 32..38 as written by `__GXSetProjection`

Note:
- This testcase uses the orthographic parameters 0/1/0/1/0/1 so all relevant floats
  are exactly representable, avoiding cross-platform float rounding drift.

