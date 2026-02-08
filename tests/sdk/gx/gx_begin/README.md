# GXBegin / GXEnd

MP4 callsite (wipe):

- `GXBegin(GX_QUADS, GX_VTXFMT0, 4)` then 4x `GXPosition2u16` then `GXEnd()`.
  Evidence: `/Users/chrislamark/projects/gamecube/decomp_mario_party_4/src/game/wipe.c`

SDK reference behavior:

- `GXBegin` writes to the GP FIFO:
  - `GX_WRITE_U8(vtxfmt | type)`
  - `GX_WRITE_U16(nverts)`
  Evidence: `/Users/chrislamark/projects/gamecube/decomp_mario_party_4/src/dolphin/gx/GXGeometry.c`

- `GXEnd` is a macro barrier in headers; for our deterministic model it is a no-op
  (we only validate the begin header stream here).

Oracle:
- expected: PPC DOL built from the reference behavior (run in Dolphin)
- actual: host build using `/Users/chrislamark/projects/gamecube/src/sdk_port/gx/GX.c`

