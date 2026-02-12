# Test Layout Migration Plan

Status: in progress  
Branch: `codex/macos-codex5.3-migrate-tests-layout`

## Goal

Clarify test intent by separating:

- trace harvest artifacts
- callsite-driven SDK tests
- property/PBT tests

without breaking existing scripts during migration.

## Target layout

- `tests/trace-harvest/`
  - retail RVZ trace hits and probe captures (current `tests/traces/`)
- `tests/sdk/callsite/`
  - decomp-callsites-derived SDK suites (current `tests/sdk/<subsystem>/<function>/`)
- `tests/sdk/pbt/`
  - property/PBT suites and related helpers (current split across `tests/pbt/` and `tests/sdk/*/property/`)

## Rollout phases

1. Define mapping + policy (this file)
2. Scaffold target directories (no moves yet)
3. Migrate `tests/traces` -> `tests/trace-harvest` with compatibility symlink
4. Migrate SDK callsite suites under `tests/sdk/callsite`
5. Migrate PBT/property suites under `tests/sdk/pbt`
6. Retarget scripts to new paths (keep fallback for old paths)
7. Run gates, update docs, then remove compatibility links later

## Compatibility policy

- During migration, preserve old paths via symlinks.
- Scripts should prefer new paths first; old path fallback allowed short-term.
- No destructive delete of old paths until replay/test gates are green.

## Initial path mapping

- `tests/traces/*` -> `tests/trace-harvest/*`
- `tests/sdk/<subsystem>/<function>/*` -> `tests/sdk/callsite/<subsystem>/<function>/*`
- `tests/pbt/*` -> `tests/sdk/pbt/*`
- `tests/sdk/*/property/*` -> `tests/sdk/pbt/<subsystem>/*`
