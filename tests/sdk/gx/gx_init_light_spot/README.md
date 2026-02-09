# GXInitLightSpot

MP4 callsite: `decomp_mario_party_4/src/game/hsfdraw.c` uses `GXInitLightSpot(&light, 20.0f, GX_SP_COS)`.

Oracle:
- expected: DOL in Dolphin
- actual: host scenario using `src/sdk_port/gx/GX.c`

We record the resulting `a[0..2]` float bit patterns.
