# VI Notes (Facts Only)

Rules:
- Facts only. No guesses.
- Every bullet must cite evidence (decomp path + symbol, or a test id + expected.bin).

## VISetNextFrameBuffer
- Callsites were enumerated across MP4/TP/WW/AC.
  Evidence: docs/sdk/vi/VISetNextFrameBuffer.md
- DOL scaffolding tests exist, but current ones only prove control-flow (marker written).
  They do NOT yet validate real VI register programming or HorVer side effects.
  Evidence: tests/sdk/vi/vi_set_next_frame_buffer/dol/*; tests/expected/vi_set_next_frame_buffer_{min,realistic_mp4,edge_unaligned}_001.bin

## Known Invariants

## Undocumented Quirks

