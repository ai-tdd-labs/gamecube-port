# GXInitTexObj (MP4 wipe)

MP4 callsite (wipe):

- `GXInitTexObj(&tex, wipe->copy_data, wipe->w, wipe->h, GX_TF_RGB565, GX_CLAMP, GX_CLAMP, GX_FALSE)`
  Evidence: `<DECOMP_MP4_ROOT>/src/game/wipe.c`

SDK reference behavior:

- Initializes the 0x20-byte internal texobj struct (`__GXTexObjInt`) by packing:
  - `mode0` wrap/filter bits + macro-set bits
  - `image0` (width-1, height-1, format low nibble)
  - `image3` (image base = image_ptr >> 5)
  - `fmt`, `loadFmt`, `loadCnt`, `flags`
  Evidence: `<DECOMP_MP4_ROOT>/src/dolphin/gx/GXTexture.c`

Oracle:
- expected: PPC DOL built from the reference behavior (run in Dolphin)
- actual: host build using `<REPO_ROOT>/src/sdk_port/gx/GX.c`
