# GXLoadNrmMtxImm

MP4 callsite-driven test for `GXLoadNrmMtxImm`.

Primary MP4 usage: `src/game/hsfdraw.c` loads the normal matrix before skinning/lighting.

This suite validates the observable FIFO command packing (u8=0x10, u32 reg) and the 3x3 payload extracted from the top-left 3x3 of the 3x4 input.
