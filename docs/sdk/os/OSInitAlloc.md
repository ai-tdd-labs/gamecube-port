# OSInitAlloc

## API

Header: `decomp_mario_party_4/include/dolphin/os/OSAlloc.h`

```c
void *OSInitAlloc(void *arenaStart, void *arenaEnd, int maxHeaps);
```

## Purpose

Initialize the OS heap subsystem using a caller-provided arena range.

This function:
- Treats `arenaStart..arenaEnd` as a backing region for heap metadata + heap memory.
- Places an array of `HeapDesc` entries at `arenaStart`.
- Returns a new, 32-byte-aligned arena pointer after the metadata array.

## Observed Behavior (MP4 SDK Source)

Source of truth for behavior:
- `decomp_mario_party_4/src/dolphin/os/OSAlloc.c` (`OSInitAlloc`)

Retail (`_DEBUG` not defined) has assertions compiled out (ASSERT* macros are no-ops), but the code still performs the same computations.

Confirmed side effects (as implemented in the SDK source):
- `HeapArray = arenaStart`
- `NumHeaps = maxHeaps`
- For each heap descriptor `i in [0..NumHeaps)`:
  - `HeapArray[i].size = -1`
  - `HeapArray[i].free = 0`
  - `HeapArray[i].allocated = 0`
- `__OSCurrHeap = -1`
- Computes and stores:
  - `ArenaStart = align_up(arenaStart + maxHeaps * sizeof(HeapDesc), 32)`
  - `ArenaEnd = align_down(arenaEnd, 32)`
- Returns `ArenaStart`

Notes:
- `sizeof(HeapDesc)` in the SDK source is 12 bytes (`long size; Cell *free; Cell *allocated;`).
- Alignment is 32 bytes.

## Callsites

Mario Party 4 init flow (game code):
- `decomp_mario_party_4/src/game/init.c` (`InitMem`): calls `OSInitAlloc(arena_lo, arena_hi, 1)` after setting arena pointers.

Runtime support code:
- `decomp_mario_party_4/src/Runtime.PPCEABI.H/GCN_Mem_Alloc.c`: calls `OSInitAlloc(arenaLo, arenaHi, 1)` as part of heap init.

## Tests (Dolphin expected vs host actual)

Canonical test location:
- `tests/sdk/os/os_init_alloc/`

Each test writes a compact “result struct” into RAM at `0x80300000` so the default dump region (`0x80300000..0x80300040`) is sufficient.

