# GXSetNumIndStages

- Subsystem: GX
- Callsite basis: MP4 GX init path (`GXInit.c`) sets indirect stage count.
- Behavior from decomp (`GXBump.c`):
  - `genMode` bits 16..18 = `nIndStages`
  - `dirtyState |= 6`
  - no immediate BP register write
