# GXGetTexBufferSize

MP4 callsite (wipe cross-fade):

- `GXGetTexBufferSize(wipe->w, wipe->h, GX_TF_RGB565, GX_FALSE, 0)`
  Evidence: `<DECOMP_MP4_ROOT>/src/game/wipe.c`

This suite validates the deterministic return value (buffer size in bytes) for the
MP4 init-style `GXRenderModeObj` dimensions (`w=640`, `h=480`) and `GX_TF_RGB565`.

Oracle:
- expected: PPC DOL built from SDK logic (run in Dolphin)
- actual: host build using `<REPO_ROOT>/src/sdk_port/gx/GX.c`
