# GXSetDrawDone (MP4 Hu3DExec)

MP4 `Hu3DExec()` conditionally calls `GXSetDrawDone()` during camera iteration.

This suite validates a deterministic, observable subset:
- BP write `0x45000002`
- `draw_done_flag` reset to 0

Note: the real SDK waits for a PE finish interrupt to flip the flag; this repo models
that separately via `GXWaitDrawDone()` as a deterministic no-block shim.
