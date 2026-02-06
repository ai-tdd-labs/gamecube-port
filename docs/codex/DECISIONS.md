# Codex Decisions

Decisions are for explaining *why* we concluded something.
Each entry must link to evidence (decomp path, test id, expected.bin).

## 2026-02-06: Separate SDK Port vs Game Workloads

Decision:
- Keep Nintendo SDK implementations under `src/sdk_port/`.
- Keep unmodified game code under `src/game_workload/<game>/` as a workload / integration driver.
- Do not copy game code into `src/sdk_port/` and do not change game logic (only minimal build glue if needed).

Why:
- Avoid mixing "spec/workload" (game code) with "product under test" (SDK port).
- Keep the evidence loop clean: SDK behavior is validated by `tests/sdk/** expected.bin vs actual.bin`.

Evidence:
- Callsite-driven SDK tests exist and are bit-exact against Dolphin for OS arena functions.
  Evidence: tests/sdk/os/os_set_arena_lo/expected/*.bin + tests/sdk/os/os_set_arena_lo/actual/*.bin
  Evidence: tests/sdk/os/os_get_arena_lo/expected/*.bin + tests/sdk/os/os_get_arena_lo/actual/*.bin
