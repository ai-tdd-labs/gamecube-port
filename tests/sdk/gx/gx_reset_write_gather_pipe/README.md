# GXResetWriteGatherPipe

- Subsystem: GX
- Callsite basis: MP4 `hsfdraw.c` calls `GXResetWriteGatherPipe()` before matrix FIFO writes.
- Behavior basis: decomp `GXMisc.c` updates PPC write-gather hardware registers only.

Host policy:
- sdk_port models no PPC write-gather hardware registers, so function is a no-op.
- Test asserts no modeled GX state mutation (`gc_gx_bp_sent_not` remains unchanged).
