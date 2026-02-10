# GXBegin / GXEnd

MP4 callsite (wipe):

- `GXBegin(GX_QUADS, GX_VTXFMT0, 4)` then 4x `GXPosition2u16` then `GXEnd()`.
  Evidence: `<DECOMP_MP4_ROOT>/src/game/wipe.c`

SDK reference behavior:

- `GXBegin` writes to the GP FIFO:
  - `GX_WRITE_U8(vtxfmt | type)`
  - `GX_WRITE_U16(nverts)`
  Evidence: `<DECOMP_MP4_ROOT>/src/dolphin/gx/GXGeometry.c`

- `GXEnd` is a macro barrier in headers; for our deterministic model it is a no-op
  (we only validate the begin header stream here).

Oracle:
- expected: PPC DOL built from the reference behavior (run in Dolphin)
- actual: host build using `<REPO_ROOT>/src/sdk_port/gx/GX.c`
