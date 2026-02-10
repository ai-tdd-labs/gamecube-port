# GXSetTevColor (MP4 wipe)

MP4 callsite (wipe):

- `GXSetTevColor(GX_TEVSTAGE1, color)` (numeric `1` matches `GX_TEVREG0` / `C0`).
  Evidence: `<DECOMP_MP4_ROOT>/src/game/wipe.c`

SDK reference behavior:

- Packs 2 BP/RAS registers and writes them to the FIFO:
  - `regRA`: (r, a, addr = 224 + id*2)
  - `regBG`: (b, g, addr = 225 + id*2) written 3x after the first write
  Evidence: `<DECOMP_MP4_ROOT>/src/dolphin/gx/GXTev.c`

Oracle:
- expected: PPC DOL built from the reference behavior (run in Dolphin)
- actual: host build using `<REPO_ROOT>/src/sdk_port/gx/GX.c`
