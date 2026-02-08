# GXSetTexCopyDst (MP4 Hu3DShadowExec)

MP4 `Hu3DShadowExec()` configures a texture copy destination after drawing shadow models:

```
if (Hu3DShadowData.unk_02 <= 0xF0) {
  GXSetTexCopyDst(unk_02, unk_02, GX_CTF_R8, 1);
} else {
  GXSetTexCopyDst(unk_02, unk_02, GX_CTF_R8, 0);
}
```

Evidence:
- `decomp_mario_party_4/src/game/hsfman.c` (`Hu3DShadowExec`)
- SDK implementation: `decomp_mario_party_4/src/dolphin/gx/GXFrameBuf.c:GXSetTexCopyDst`

This suite validates the observable packed BP/RAS register mirrors:
- `cpTex` (mipmap bit + format fields used by GXCopyTex)
- `cpTexStride` (high byte 0x4D; rowTiles * cmpTiles)
- `cpTexZ` (Z texture flag)

Note:
- We target the MP4 callsite `GX_CTF_R8` which results in deterministic tile math.

