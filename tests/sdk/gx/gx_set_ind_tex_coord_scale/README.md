# GXSetIndTexCoordScale

- Subsystem: GX
- Callsite basis: MP4 GX init uses stage scales `GX_ITS_1` for stages 0..3.
- Decomp behavior (`GXBump.c`):
  - stage0/1 update `IndTexScale0` (reg id `0x25`)
  - stage2/3 update `IndTexScale1` (reg id `0x26`)
  - writes BP register and clears `bpSentNot`.
