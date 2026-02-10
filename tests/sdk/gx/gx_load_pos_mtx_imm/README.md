# GXLoadPosMtxImm

MP4 callsites (wipe):

- `GXLoadPosMtxImm(modelview, GX_PNMTX0)` where `modelview` is identity.
  Evidence: `<DECOMP_MP4_ROOT>/src/game/wipe.c`

SDK reference behavior:

- Writes to the GP FIFO:
  1. `GX_WRITE_U8(0x10)`
  2. `GX_WRITE_U32((id * 4) | 0xB0000)`
  3. `WriteMTXPS4x3(mtx, &GXWGFifo.f32)` (12 floats, row-major)
  Evidence: `<DECOMP_MP4_ROOT>/src/dolphin/gx/GXTransform.c`

Oracle:
- expected: PPC DOL built from the reference behavior (run in Dolphin)
- actual: host build using `<REPO_ROOT>/src/sdk_port/gx/GX.c`
