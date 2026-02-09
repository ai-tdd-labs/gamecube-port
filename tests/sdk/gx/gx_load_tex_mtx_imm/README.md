# GXLoadTexMtxImm

MP4 callsite-driven tests for `GXLoadTexMtxImm`.

MP4 uses both types:
- `GX_MTX2x4` for 2D texcoord matrices (e.g. `GX_TEXMTX7`).
- `GX_MTX3x4` for 3D matrices (e.g. `GX_TEXMTX9`).

This suite validates the observable FIFO command packing (u8=0x10, u32 reg) and the payload words.
