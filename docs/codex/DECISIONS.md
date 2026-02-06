# Decisions

This file documents decisions we made and why.

## Legacy test migration policy

Decision:
- Do not blindly copy legacy DOL tests into `tests/sdk/`.
- Only treat a legacy test as a Dolphin oracle if the DOL executes real SDK behavior, i.e. compiled from decomp SDK sources under `decomp_mario_party_4/src/dolphin/...` (or other decomp roots).

Why:
- Several legacy tests implement "synthetic" versions of SDK functions inside the test file (mocked regs, mocked SRAM, stubbed interrupts).
- Those tests are useful as algorithm sketches, but they do not validate against Dolphin's real behavior and therefore do not satisfy "never guess".

What to do instead:
- When legacy is synthetic, rewrite the DOL test to call the decomp SDK function and dump results, then port the behavior under `src/sdk_port/` and compare expected vs actual.

