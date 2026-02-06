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
- Write confirmed behaviors to docs/codex/NOTES.md (facts only, with evidence).

## Test Locations

- New tests go under tests/sdk/<subsystem>/<function>/
- Harness templates and shared boilerplate go under tests/harness/

### Canon test layout (per SDK function)

```
tests/sdk/<subsystem>/<function>/
  dol/
    generic/<case_id>/            # minimal + edge cases (not tied to a specific game)
    mp4/<case_id>/                # realistic calls (mirrors MP4 callsites/args)
    tp/<case_id>/                 # realistic calls (mirrors TP callsites/args)
    ww/<case_id>/                 # realistic calls (mirrors WW callsites/args)
    ac/<case_id>/                 # realistic calls (mirrors AC callsites/args)
  expected/<test_name>.bin        # Dolphin oracle dumps (bit-exact)
  actual/<test_name>.bin          # Host/sdk_port dumps (bit-exact)
  README.md                       # optional: what the cases represent
```

Naming:
- Prefer `<function>_<case_id>` for the built `.dol` base name so expected bins do not collide.
- Use `generic/` for synthetic (minimal/edge) tests. Use per-game folders for realistic callsite tests.
