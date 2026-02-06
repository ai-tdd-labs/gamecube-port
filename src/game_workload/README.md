# Game Workloads

This folder holds *unmodified* game code that serves as a workload / integration driver
for the SDK port.

Rules:
- Do not change game logic here.
- Only add minimal build glue (includes, build files) if required.
- SDK behavior must be implemented under `src/sdk_port/` and validated via `tests/sdk/`.

