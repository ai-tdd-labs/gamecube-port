# OSAllocFromHeap

Header: `decomp_mario_party_4/include/dolphin/os/OSAlloc.h`

## Contract

`void *OSAllocFromHeap(OSHeapHandle heap, u32 size);`

- Allocates `size` bytes from heap `heap`.
- Returns pointer to the user region (header is 0x20 bytes before the returned pointer).
- Alignment: 32 bytes.

Evidence (MP4 SDK source): `decomp_mario_party_4/src/dolphin/os/OSAlloc.c` (`OSAllocFromHeap`).

## Callsites (decomp)

- MP4: `src/game_workload/mp4/vendor/src/game/init.c` (`HuSysInit`): `DefaultFifo = OSAlloc(0x100000);`
  Note: `OSAlloc(size)` is a macro: `OSAllocFromHeap(__OSCurrHeap, size)`.

## Deterministic tests

Dolphin expected vs host actual:
- `tests/sdk/os/os_alloc_from_heap/dol/generic/min_001`
- `tests/sdk/os/os_alloc_from_heap/dol/mp4/realistic_fifo_001`

