# GXWaitDrawDone (MP4 Hu3DExec)

MP4 `Hu3DExec()` uses `GXWaitDrawDone()` during camera iteration.

The real SDK blocks until a PE finish interrupt sets a flag.
This repo models it as a deterministic no-block shim:
- increments a counter
- sets `draw_done_flag = 1`

This keeps unit tests and host runtime deterministic.
