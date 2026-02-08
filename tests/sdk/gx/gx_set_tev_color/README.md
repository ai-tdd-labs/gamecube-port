# GXSetTevColor (MP4 wipe)

MP4 callsite (wipe):

- `GXSetTevColor(GX_TEVSTAGE1, color)` (numeric `1` matches `GX_TEVREG0` / `C0`).
  Evidence: `/Users/chrislamark/projects/gamecube/decomp_mario_party_4/src/game/wipe.c`

SDK reference behavior:

- Packs 2 BP/RAS registers and writes them to the FIFO:
  - `regRA`: (r, a, addr = 224 + id*2)
  - `regBG`: (b, g, addr = 225 + id*2) written 3x after the first write
  Evidence: `/Users/chrislamark/projects/gamecube/decomp_mario_party_4/src/dolphin/gx/GXTev.c`

Oracle:
- expected: PPC DOL built from the reference behavior (run in Dolphin)
- actual: host build using `/Users/chrislamark/projects/gamecube/src/sdk_port/gx/GX.c`

