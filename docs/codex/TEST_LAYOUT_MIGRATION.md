# Test Layout Migration Plan

Status: in progress  
Branch: `codex/macos-codex5.3-migrate-tests-layout`

## Goal

Clarify test intent by separating:

- trace harvest artifacts
- callsite-driven SDK tests
- property/PBT tests

## Target layout

- `tests/trace-harvest/`
  - retail RVZ trace hits and probe captures
- `tests/sdk/callsite/`
  - decomp-callsites-derived SDK suites
- `tests/sdk/pbt/`
  - property/PBT suites and related helpers

## Rollout phases

1. Define mapping + policy - DONE
2. Scaffold target directories - DONE
3. Migrate `tests/traces` -> `tests/trace-harvest` - DONE
4. Migrate SDK callsite suites under `tests/sdk/callsite` - DONE (compatibility mirrors via symlinks)
5. Migrate PBT/property suites under `tests/sdk/pbt` - DONE (compatibility mirrors via symlinks)
6. Retarget scripts/docs to new paths - DONE for replay/tooling and core docs
7. Validate gates + remove old alias - DONE (`tests/traces` removed)

## Implemented on this branch

- Trace corpus moved to `tests/trace-harvest/`
- Old `tests/traces` path removed
- `tests/sdk/callsite/<subsystem>` mirrors current sdk suite folders via symlinks
- `tests/sdk/pbt/legacy` points to `tests/pbt`
- `tests/sdk/pbt/<subsystem>/...` mirrors current property suites via symlinks
- Replay tooling prefers `tests/trace-harvest/...`:
  - `tools/replay_trace_suite.sh`
  - `tools/run_replay_gate.sh`

## Initial mapping (old -> new)

- `tests/traces/*` -> `tests/trace-harvest/*`
- `tests/sdk/<subsystem>/<function>/*` -> `tests/sdk/callsite/<subsystem>/<function>/*`
- `tests/pbt/*` -> `tests/sdk/pbt/legacy/*`
- `tests/sdk/*/property/*` -> `tests/sdk/pbt/<subsystem>/*`
