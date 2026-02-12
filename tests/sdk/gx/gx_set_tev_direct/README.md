# GXSetTevDirect

- Subsystem: GX
- Callsite basis: MP4 GX init sets all TEV stages to direct mode.
- Decomp behavior (`GXBump.c`): `GXSetTevDirect(stage)` forwards to
  `GXSetTevIndirect(stage, ..., all direct defaults)` and writes BP reg id `(stage + 16)`.
