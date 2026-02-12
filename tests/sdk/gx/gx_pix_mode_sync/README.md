# GXPixModeSync

- Subsystem: GX
- Callsite basis: MP4 init GX flow (`InitRenderMode`) where pixel engine control state is flushed.
- Scenario: set `peCtrl` via `GXSetZCompLoc(1)`, then call `GXPixModeSync()`.

Expected behavior:
- `GXPixModeSync()` writes current `peCtrl` BP value.
- `gc_gx_bp_sent_not` is cleared to `0`.

Artifacts:
- Expected dump: `tests/sdk/gx/gx_pix_mode_sync/expected/gx_pix_mode_sync_mp4_init_gx_001.bin`
- Actual dump: `tests/sdk/gx/gx_pix_mode_sync/actual/gx_pix_mode_sync_mp4_init_gx_001.bin`
