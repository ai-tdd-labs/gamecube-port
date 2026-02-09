# GXPosition2u16 (MP4 callsite-style)

This suite mirrors how MP4 uses `GXPosition2u16` in `wipe.c` (u16 coords).

Oracle:
- `dol/*` runs in Dolphin to produce `expected/*.bin`
- `host/*` runs against `src/sdk_port` to produce `actual/*.bin`
- `tools/ram_compare.py expected actual` must be bit-exact

