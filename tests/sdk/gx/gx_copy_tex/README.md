# GXCopyTex (MP4 Hu3DShadowExec)

MP4 `Hu3DShadowExec()` copies the rendered shadow buffer into a texture:

```
GXCopyTex(Hu3DShadowData.unk_04, 1);
```

Evidence:
- `decomp_mario_party_4/src/game/hsfman.c` (`Hu3DShadowExec`)
- SDK implementation: `decomp_mario_party_4/src/dolphin/gx/GXFrameBuf.c:GXCopyTex`

This suite validates the observable sequence affecting copy registers:
- Writes of `cpTexSrc`, `cpTexSize`, `cpTexStride`
- Address BP reg (0x4B, `phyAddr>>5`)
- `cpTex` write (0x52) with clear bit set
- Conditional zmode/cmode0 writes when clear=1 (observable as last-raster writes)

Note:
- We don't emulate full PE state; the deterministic oracle tracks the packed regs written.

