# GXLoadTexObj (MP4 wipe)

MP4 callsite (wipe):

- `GXInitTexObj(&tex, wipe->copy_data, wipe->w, wipe->h, GX_TF_RGB565, GX_CLAMP, GX_CLAMP, GX_FALSE)`
- `GXLoadTexObj(&tex, GX_TEXMAP0)`
  Evidence: `<DECOMP_MP4_ROOT>/src/game/wipe.c`

SDK reference behavior:

- `GXLoadTexObj` selects a `GXTexRegion` via the `texRegionCallback` and then calls `GXLoadTexObjPreLoaded`.
- `GXLoadTexObjPreLoaded` sets the register IDs for the chosen texture map and writes:
  - `mode0`, `mode1`, `image0`, `image1`, `image2`, `image3` as BP/RAS regs
  Evidence: `<DECOMP_MP4_ROOT>/src/dolphin/gx/GXTexture.c`
- Default callback behavior is installed in `GXInit` and round-robins over 8 cache regions.
  Evidence: `<DECOMP_MP4_ROOT>/src/dolphin/gx/GXInit.c`

Oracle:
- expected: PPC DOL built from the reference behavior (run in Dolphin)
- actual: host build using `<REPO_ROOT>/src/sdk_port/gx/GX.c`
