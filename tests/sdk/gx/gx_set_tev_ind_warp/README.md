# GXSetTevIndWarp

- Subsystem: GX
- Callsite basis: MP4 indirect TEV configuration paths.
- Decomp behavior (`GXBump.c`):
  - chooses wrap mode from `replace_mode` (`OFF` vs `0`)
  - chooses bias from `signed_offset` (`NONE` vs `STU`)
  - forwards to `GXSetTevIndirect` packed BP register write.
