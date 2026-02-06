# Agent Instructions

We are reverse-engineering and porting a Nintendo GameCube game using decompiled code and deterministic unit tests.

Codex has no memory. The repository is the memory.

=====================
GOAL
=====================

Create a workflow where Codex:
- learns the Nintendo SDK and game callsites by committing notes,
- reconstructs SDK behavior via unit tests,
- validates behavior against Dolphin,
- becomes faster because its own prior work lives in the repo.

=====================
REPO MEMORY LAYOUT
=====================

Always use and update these folders:

docs/
  codex/
    NOTES.md        # facts only
    WORKFLOW.md     # repeatable procedures
    DECISIONS.md    # why conclusions were made
    GLOSSARY.md     # SDK terms, struct names

docs/
  sdk/
    os/
    dvd/
    gx/
    vi/
    pad/

tests/
  harness/
  sdk/

tools/
  run_tests.sh
  dump_expected.sh
  diff_bins.sh
  helpers/

src/
  sdk_port/         # reimplemented SDK functions
  game_workload/    # unmodified game code used as workload/integration driver (do not change logic)

Everything important MUST be written to disk.

=====================
GENERAL RULES
=====================

1. NEVER GUESS.
   Every claim must be backed by:
   - decompiled code
   - disassembly / symbols
   - Dolphin behavior (expected.bin)
   - memory dumps / register state

2. Always separate:
   - SDK API contract
   - internal implementation
   - side effects (memory, registers, globals)

3. Tests ALWAYS come before implementation changes.

4. Each session ends with a commit.

=====================
STANDARD WORKFLOW
=====================

For any SDK function (example: DVDInit, OSInit, GXSetCopyFilter):

STEP 1 - SCOPING
- Pick ONE SDK function.
- Identify subsystem (OS, DVD, GX, VI, PAD).

STEP 2 - CALLSITE RESEARCH
- Find all callsites in each game decomp (MP4/TP/WW/AC).
- Note:
  - call order
  - arguments
  - frequency (init-only vs runtime)

STEP 3 - BEHAVIOR DISCOVERY
- Determine:
  - memory writes
  - register writes
  - global state changes
- Identify idempotency and edge cases.

STEP 4 - UNIT TESTS
- Write at least 3 deterministic tests:
  - minimal call
  - realistic game call
  - edge case (double call, invalid state)
- Tests must:
  - initialize memory
  - call the function
  - dump memory/register regions

STEP 5 - EXPECTED VS ACTUAL
- Produce expected.bin via Dolphin.
- Produce actual.bin via your SDK port.
- Diff:
  - first mismatch offset
  - affected memory region
  - likely cause

STEP 6 - IMPLEMENTATION
- Modify SDK implementation minimally.
- No refactors.
- Repeat until expected == actual.

STEP 7 - NOTES (LEARNING)
- Write findings to docs/codex/NOTES.md:
  - confirmed side effects
  - invariants
  - undocumented behavior

STEP 8 - HELPERS (OPTIONAL)
If something repeats (dump boilerplate, diff interpretation), create helpers under tools/helpers/.

=====================
SESSION CLOSURE
=====================

At the end of EVERY session:
1) Update docs/codex/NOTES.md
2) Update or add tests
3) Commit changes
