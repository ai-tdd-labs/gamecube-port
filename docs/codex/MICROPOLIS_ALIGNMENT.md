# Micropolis vs GameCube PBT Alignment

Date: 2026-02-11

This file resolves confusion about "true PBT" vs "randomized parity tests" and
documents concrete actions in this repo.

## What Micropolis does

1. External oracle process (separate binary, IPC/files)
2. Full-system integration (tick/world level)
3. Framework PBT (fast-check style shrinking/arbitraries)
4. Replay fixtures
5. Serialized state outputs

## What we currently do

1. Inline oracle linking (same binary as candidate)
2. Subsystem-level suites (OSAlloc/OSThread/MTX/etc.)
3. Deterministic randomized parity + property assertions (custom harness)
4. Retail trace replay exists for selected functions
5. Mostly in-memory compare + binary dumps; limited explicit serialization

## Fixes applied now

1. Added replay gate runner:
   - `tools/run_replay_gate.sh`
2. Added deterministic failure minimizer helper:
   - `tools/pbt_minimize_failure.sh`
3. Kept terminology explicit:
   - In this repo, "PBT" means deterministic randomized property/parity harnesses,
     not a third-party shrinking framework.

## Gaps still open

1. External oracle process mode is not yet standard for all suites.
2. No single "full game tick" oracle harness yet (we use chain-smoke + replay).
3. Automatic shrinking is helper-level (best-effort), not framework-grade.
4. State serialization is partial and suite-specific.

## Practical policy

- Keep current deterministic parity harnesses (fast and useful).
- For critical paths, pair with replay-gate and chain-smoke.
- Use `tools/pbt_minimize_failure.sh` on regressions to produce minimal repros.
- Add external-oracle mode only where inline oracle causes coupling risk.
