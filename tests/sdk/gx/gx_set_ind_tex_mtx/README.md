# GXSetIndTexMtx

- Subsystem: GX
- Callsite basis: MP4 indirect texture setup paths.
- Decomp behavior (`GXBump.c`):
  - map `mtx_id` to slot id
  - emit three packed BP regs (`id*3 + 6/7/8`)
  - clear `bpSentNot`.
