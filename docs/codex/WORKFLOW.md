# Codex Workflow

This repository is the memory.

## Core Loop (for a single SDK function)

1) Scoping
- Pick exactly one SDK function.
- Record subsystem and signature.

2) Callsite research
- List all callsites per game decomp (MP4/TP/WW/AC).
- Record call order, arguments, and frequency.

3) Behavior discovery
- Record side effects (memory, globals, registers).
- Record preconditions and edge cases.

4) Unit tests (deterministic)
- At least 3 tests:
  - minimal call
  - realistic game call
  - edge case
- Each test must produce an expected.bin from Dolphin.

5) Expected vs actual
- Run host/runtime port to produce actual.bin.
- Diff expected.bin vs actual.bin.

6) Implementation
- Minimal changes only.
- Iterate until bit-exact match.

7) Notes
- Write confirmed behaviors to docs/codex/<subsystem>.md.
- Keep docs/codex/NOTES.md as an index + high-level changelog.

## Test Locations

- New tests go under tests/sdk/<subsystem>/<function>/
- Harness templates and shared boilerplate go under tests/harness/
