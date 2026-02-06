# Codex Notes (Index)

Codex has no memory. The repository is the memory.

Rules:
- Facts only. No guesses.
- Every bullet must cite evidence (decomp path + symbol, or a test id + expected.bin).

Subsystem notes (write facts in these files, not here):
- OS: docs/codex/os.md
- DVD: docs/codex/dvd.md
- GX: docs/codex/gx.md
- VI: docs/codex/vi.md
- PAD: docs/codex/pad.md

## Changelog (high level)

### 2026-02-06
- Created repo-memory workflow skeleton: docs/codex/*, docs/sdk/*, tools/*.sh.
- Started VISetNextFrameBuffer callsite research and scaffolded 3 DOL tests.
  Evidence: docs/sdk/vi/VISetNextFrameBuffer.md; tests/expected/vi_set_next_frame_buffer_{min,realistic_mp4,edge_unaligned}_001.bin
