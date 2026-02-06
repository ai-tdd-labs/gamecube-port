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

### Host actual.bin (SDK port)

After `expected/` exists, implement the minimal SDK port for the function under `src/sdk_port/`
and add a matching host testcase under `tests/sdk/<subsystem>/<function>/host/`.

Commands:
1) Dump expected: `tools/run_tests.sh expected tests/sdk/<subsystem>/<function>`
2) Produce actual: `tools/run_host_scenario.sh tests/sdk/<subsystem>/<function>/host/<case>_scenario.c`
3) Diff: `tools/diff_bins.sh tests/sdk/<subsystem>/<function>/expected/<case>.bin tests/sdk/<subsystem>/<function>/actual/<case>.bin`

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

## Notes Format (Facts Only)

When adding facts to `docs/codex/NOTES.md`, prefer this shape so future Codex runs can grep quickly:

```
### <FunctionName>
- Contract: <one sentence>
  Evidence: <header or decomp path>
- Side effects: <bullets, memory/globals/regs>
  Evidence: <decomp path or expected/actual bins>
- Callsites: <MP4/TP/WW/AC pointers>
  Evidence: docs/sdk/<subsystem>/<FunctionName>.md
- Testcases:
  - <case_id>: <what it represents>
    Evidence: tests/sdk/<subsystem>/<function>/expected/<case>.bin (+ actual if available)
```

For repeated runs, log PASS/FAIL outcomes via:
- `tools/helpers/record_test_result.py` (appends an evidence line to `docs/codex/NOTES.md`)
