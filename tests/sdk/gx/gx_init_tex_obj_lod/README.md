# GXInitTexObjLOD (MP4 pfDrawFonts)

MP4 callsite (printfunc):

- `GXInitTexObjLOD(&font_tex, GX_NEAR, GX_NEAR, 0, 0, 0, GX_FALSE, GX_FALSE, GX_ANISO_1)`
  Evidence: `/Users/chrislamark/projects/gamecube/decomp_mario_party_4/src/game/printfunc.c`

SDK reference behavior:

- Updates the existing 0x20-byte internal texobj struct by packing LOD and filter bits into `mode0` and `mode1`.
  Evidence: `/Users/chrislamark/projects/gamecube/decomp_mario_party_4/src/dolphin/gx/GXTexture.c:GXInitTexObjLOD`

Oracle:
- expected: PPC DOL built from the reference behavior (run in Dolphin)
- actual: host build using `/Users/chrislamark/projects/gamecube/src/sdk_port/gx/GX.c`
