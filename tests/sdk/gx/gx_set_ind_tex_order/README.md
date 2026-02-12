# GXSetIndTexOrder

- Subsystem: GX
- Callsite basis: MP4 GX init path sets indirect texture stage orders.
- Decomp behavior (`GXBump.c`):
  - updates `iref` pairs for selected stage (`tex_map`, `tex_coord`)
  - writes BP reg (`iref`), sets `dirtyState |= 3`, clears `bpSentNot`.
