# GXSetTexCopySrc (MP4 Hu3DShadowExec)

MP4 `Hu3DShadowExec()` configures a texture copy source after drawing shadow models:

```
if (Hu3DShadowData.unk_02 <= 0xF0) {
  GXSetTexCopySrc(0, 0, unk_02 * 2, unk_02 * 2);
} else {
  GXSetTexCopySrc(0, 0, unk_02, unk_02);
}
```

Evidence:
- `decomp_mario_party_4/src/game/hsfman.c` (`Hu3DShadowExec`)
- SDK implementation: `decomp_mario_party_4/src/dolphin/gx/GXFrameBuf.c:GXSetTexCopySrc`

This suite validates the observable packed BP/RAS register mirrors:
- `cpTexSrc` (high byte 0x49)
- `cpTexSize` (high byte 0x4A)
- `bpSentNot` behavior

