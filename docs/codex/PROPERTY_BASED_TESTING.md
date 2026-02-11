# Property-Based Testing (PBT) in This Repo

Goal: add a *second* net under the existing bit-exact snapshot tests.

OSAlloc-style standardization reference:
- `docs/codex/PBT_OSALLOC_STANDARD_MATRIX.md`

We do **not** replace:
- DOL (Dolphin) expected.bin vs host actual.bin
- Retail trace replay (RVZ break at entry/exit, then replay on host)

Instead, PBT is used when a function has clear invariants and a large input
space that is cheap to explore.

## What PBT means (short)

Snapshot tests: "for this concrete input snapshot, output bytes must match".

PBT: "for many generated inputs, these properties must always hold".

If a property fails, the harness prints a *counterexample* input.

## Where PBT helps for GameCube SDK ports

Good candidates:
- Rounding / alignment helpers (pure-ish):
  - OSRoundUp32B / OSRoundDown32B
- Clamp / range enforcement helpers (mostly local):
  - PADClamp-like logic (when modeled deterministically)

Poor candidates:
- Side-effect heavy functions with hidden global state:
  - SITransfer / DVD / VI callbacks / interrupt masking

Those are better validated with snapshot or retail trace replay.

## Example properties (OSRoundUp32B)

For any `u32 x`:
- `OSRoundUp32B(x) % 32 == 0`
- `OSRoundUp32B(x) >= x`
- `OSRoundUp32B(OSRoundUp32B(x)) == OSRoundUp32B(x)` (idempotent)

For any `u32 x`:
- `OSRoundDown32B(x) % 32 == 0`
- `OSRoundDown32B(x) <= x`
- `OSRoundDown32B(OSRoundDown32B(x)) == OSRoundDown32B(x)` (idempotent)

Cross-check:
- `OSRoundDown32B(OSRoundUp32B(x)) == OSRoundUp32B(x)` or `== OSRoundDown32B(x)` depending on definition.

## How we run PBT here

We keep it minimal and deterministic:
- No external PBT framework required.
- Use a fixed PRNG seed so failing inputs are reproducible.
- Run on host only (fast), and treat failures as "stop the line".

Directory convention:
- `tests/pbt/<subsystem>/<name>/...`

## Acceptance bar

PBT is **not** a ground truth oracle.

It is a safety net:
- If PBT fails: the port is almost certainly wrong (or the property is wrong).
- If PBT passes: it increases confidence, but does not prove bit-exact parity.
