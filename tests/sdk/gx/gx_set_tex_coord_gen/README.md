# GXSetTexCoordGen (MP4 wipe)

MP4 callsite (wipe):

- `GXSetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY)`
  Evidence: `<DECOMP_MP4_ROOT>/src/game/wipe.c`

SDK reference behavior:

- `GXSetTexCoordGen` is an inline wrapper over `GXSetTexCoordGen2(..., GX_FALSE, GX_PTIDENTITY)`.
  Evidence: `<DECOMP_MP4_ROOT>/include/dolphin/gx/GXGeometry.h`
- `GXSetTexCoordGen2` writes:
  - XF reg `0x40 + dst_coord` (texgen ctrl)
  - XF reg `0x50 + dst_coord` (postmtx/normalize)
  - updates `matIdxA`/`matIdxB` and calls `__GXSetMatrixIndex` (writes XF reg 24/25)
  Evidence: `<DECOMP_MP4_ROOT>/src/dolphin/gx/GXAttr.c`

Oracle:
- expected: PPC DOL built from the reference behavior (run in Dolphin)
- actual: host build using `<REPO_ROOT>/src/sdk_port/gx/GX.c`
