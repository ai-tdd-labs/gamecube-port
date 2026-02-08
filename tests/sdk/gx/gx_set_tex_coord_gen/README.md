# GXSetTexCoordGen (MP4 wipe)

MP4 callsite (wipe):

- `GXSetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY)`
  Evidence: `/Users/chrislamark/projects/gamecube/decomp_mario_party_4/src/game/wipe.c`

SDK reference behavior:

- `GXSetTexCoordGen` is an inline wrapper over `GXSetTexCoordGen2(..., GX_FALSE, GX_PTIDENTITY)`.
  Evidence: `/Users/chrislamark/projects/gamecube/decomp_mario_party_4/include/dolphin/gx/GXGeometry.h`
- `GXSetTexCoordGen2` writes:
  - XF reg `0x40 + dst_coord` (texgen ctrl)
  - XF reg `0x50 + dst_coord` (postmtx/normalize)
  - updates `matIdxA`/`matIdxB` and calls `__GXSetMatrixIndex` (writes XF reg 24/25)
  Evidence: `/Users/chrislamark/projects/gamecube/decomp_mario_party_4/src/dolphin/gx/GXAttr.c`

Oracle:
- expected: PPC DOL built from the reference behavior (run in Dolphin)
- actual: host build using `/Users/chrislamark/projects/gamecube/src/sdk_port/gx/GX.c`

