# GXSetFog (MP4 Hu3DFogClear)

MP4 `Hu3DFogClear()` does:
- `FogData.fogType = 0`
- `GXSetFog(GX_FOG_NONE, 0, 0, 0, 0, BGColor)`

This suite validates the BP/RAS register sequence emitted by the Dolphin SDK implementation (`decomp_mario_party_4/src/dolphin/gx/GXPixel.c:GXSetFog`).

Note: this testcase intentionally uses the `farz==nearz` / `endz==startz` branch to avoid cross-platform FP rounding issues while still covering the full bitfield packing.
