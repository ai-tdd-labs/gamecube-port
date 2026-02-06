# OSGetArenaHi

Header: `decomp_mario_party_4/include/dolphin/os/OSArena.h`

## Contract

`void *OSGetArenaHi(void);`

- Returns the current arena-high pointer.

Evidence (MP4 SDK source): `decomp_mario_party_4/src/dolphin/os/OSArena.c`.

## Callsites (decomp)

- MP4: `src/game_workload/mp4/vendor/src/game/init.c` (`InitMem`).

## Deterministic tests

Dolphin expected vs host actual:
- `tests/sdk/os/os_set_arena_hi/dol/generic/min_001` (sets then gets)
- `tests/sdk/os/os_set_arena_hi/dol/generic/edge_double_call_001`

