# OSSetCurrentHeap

Header: `decomp_mario_party_4/include/dolphin/os/OSAlloc.h`

## Contract

`OSHeapHandle OSSetCurrentHeap(OSHeapHandle heap);`

- Sets the SDK global `__OSCurrHeap` to `heap`.
- Returns the previous `__OSCurrHeap` value.

Evidence (MP4 SDK source): `decomp_mario_party_4/src/dolphin/os/OSAlloc.c` (`OSSetCurrentHeap`).

## Callsites (decomp)

- MP4: `src/game_workload/mp4/vendor/src/game/init.c` (`InitMem`).

## Deterministic tests

Dolphin expected vs host actual:
- `tests/sdk/os/os_set_current_heap/dol/generic/min_001`
- `tests/sdk/os/os_set_current_heap/dol/mp4/realistic_initmem_001`

