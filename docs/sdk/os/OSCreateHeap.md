# OSCreateHeap

## API

Header: `decomp_mario_party_4/include/dolphin/os/OSAlloc.h`

```c
OSHeapHandle OSCreateHeap(void *start, void *end);
```

## Purpose

Create a heap descriptor inside the heap system initialized by `OSInitAlloc`.

## Observed Behavior (MP4 SDK Source)

Source of truth:
- `decomp_mario_party_4/src/dolphin/os/OSAlloc.c` (`OSCreateHeap`)

Behavior:
- Aligns `start` up to 32 bytes and `end` down to 32 bytes.
- Finds the first `HeapDesc` with `size < 0` and initializes it:
  - `hd->size = end - start`
  - writes a free-list cell at `start` with:
    - `prev = 0`
    - `next = 0`
    - `size = hd->size`
  - `hd->free = start`
  - `hd->allocated = 0`
  - returns heap index
- If no free heap descriptors exist, returns `-1`.

## Callsites

MP4 init flow:
- `decomp_mario_party_4/src/game/init.c` (`InitMem`): after `OSInitAlloc`, calls `OSCreateHeap(arena_lo, arena_hi)` and stores it via `OSSetCurrentHeap`.

WW / JKRStdHeap-style:
- `decomp_wind_waker` code builds a heap from a fixed memory region after `OSInitAlloc` (varies by title/system).

## Tests (Dolphin expected vs host actual)

Canonical test location:
- `tests/sdk/os/os_create_heap/`

Current cases:
- MP4 realistic initmem: create heap from `new_lo..arena_hi`
- WW jkrstdheap-style: create heap from a fixed region `0x80010000..0x80014000`

