# GXSetCopyClear (MP4 Hu3DPreProc)

MP4 `Hu3DPreProc()` calls:
- `GXSetCopyClear(BGColor, 0xFFFFFF)`

This suite validates the BP register writes that the Dolphin SDK emits (GXFrameBuf.c):
- 0x4F: `R` + `A`
- 0x50: `B` + `G`
- 0x51: `Z` (24-bit)

Oracle model:
- DOL test runs the minimal PPC implementation derived from the decomp SDK, dumps markers.
- Host test runs `src/sdk_port` implementation, dumps the same markers.
