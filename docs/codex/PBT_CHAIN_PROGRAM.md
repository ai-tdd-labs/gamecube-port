# PBT Chain Program (Definition of Done)

This file defines when a subsystem can be called "whole-chain PBT covered".

## Scope

Subsystems in this program:

- `OSAlloc`
- `DVDFS`
- `ARQ`
- `CARD-FAT`
- `MTX`
- `OSThread+Mutex+Msg` (incl. WaitCond, invariant checkers)
- `OSStopwatch`
- `OSTime` (OSTicksToCalendarTime)
- `PADClamp` (ClampStick, ClampTrigger, PADClamp)
- `dvdqueue` (circular DL priority queue)
- `OSAlarm` (sorted temporal priority queue)
- `GXTexture` (GXGetTexBufferSize, GetImageTileCount)

## Definition of Done (DoD) per subsystem

A subsystem is only `DONE` when all checks below are green.

1. `Matrix complete`
   - Leaf functions are enumerated.
   - Important branch/error paths are enumerated.
   - Each row is mapped to one or more concrete tests.

2. `Leaf-level property coverage`
   - Harness supports targeted execution per operation/leaf (`--op=...` style).
   - Every matrix leaf row has at least one targeted property run.

3. `Stateful chain coverage`
   - Mixed multi-step sequences exist (not only single-operation checks).
   - Sequences include init -> use -> error/recovery -> shutdown style paths where relevant.

4. `Oracle linkage`
   - At least one replayable oracle source per key path:
     - deterministic PPC-vs-host smoke and/or
     - retail trace replay.
   - Commands and fixtures are documented.

5. `Mutation gate`
   - Mutation checks exist and are reproducible.
   - At least one known mutant fails per critical property.
   - Threshold policy is enforced (see below).

6. `One-button gate`
   - Subsystem is included in a one-command gate run.
   - Gate exits nonzero on any failure.

## Threshold policy

- Minimum mutation score target per subsystem: `>= 90%`.
- Any surviving mutant in a critical path blocks `DONE`.
- New leaf rows must include:
  - one targeted property test
  - one mutation check (or explicit rationale in `NOTES.md`).

## Required artifacts

For each subsystem, keep these artifacts up to date:

- Coverage matrix file under `docs/sdk/`.
- Property harness and commands under `tests/` and `tools/`.
- Oracle fixtures/traces under `tests/oracles/` or `tests/traces/`.
- Facts in `docs/codex/NOTES.md`.

## Status table

| Subsystem | Matrix | Leaf ops | Stateful | Oracle | Mutation | Gate | Status |
| --- | --- | --- | --- | --- | --- | --- | --- |
| OSAlloc | in progress | in progress | in progress | in progress | in progress | pending | not done |
| DVDFS | in progress | in progress | in progress | in progress | in progress | pending | not done |
| ARQ | in progress | in progress | in progress | in progress | in progress | pending | not done |
| CARD-FAT | in progress | in progress | in progress | in progress | in progress | pending | not done |
| MTX | in progress | in progress | in progress | in progress | in progress | pending | not done |
| OSThread+Mutex+Msg | done | done | done | done | pending | pending | not done |
| OSStopwatch | done | done | done | done | pending | pending | not done |
| OSTime | done | done | done | done | pending | pending | not done |
| PADClamp | done | done | done | done | pending | pending | not done |
| dvdqueue | done | done | done | done | pending | pending | not done |
| OSAlarm | done | done | done | done | pending | pending | not done |
| GXTexture | done | done | done | done | pending | pending | not done |

## Execution order (global)

1. Complete matrix tasks.
2. Implement/expand leaf-level ops.
3. Add oracle linkage.
4. Enforce mutation gate.
5. Wire one-button global gate.
6. Update notes and close issue.
