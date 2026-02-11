# OSAlloc Property Parity (portable)

Fast, deterministic, host-only parity checks for the GameCube SDK heap allocator.

This suite compares:
- **Oracle**: allocator behavior derived from the MP4 decomp, implemented with **32-bit offset pointers** in an arena buffer.
- **Port**: `src/sdk_port/os/OSAlloc.c` running against emulated big-endian memory via `gc_mem`.

It is intentionally **portable** (no `-m32`), so it runs on macOS + Windows + Linux.

## What this is (and isn't)

- This is a **semantic parity** harness:
  - alloc success/failure parity
  - returned **relative offsets** parity (from heap base)
  - `OSCheckHeap()` free-byte count parity after every step
- This is **not** a Dolphin RAM-dump oracle. For bit-exact memory dumps and
  hardware-ish behavior, use the DOL + expected.bin workflow.

## Oracle reference

Behavioral reference:

`external/mp4-decomp/src/dolphin/os/OSAlloc.c`

The oracle uses 32-bit offsets (0 == NULL) instead of native pointers so it can
run on 64-bit hosts without `-m32`.

## How to run

```bash
# Default: 200 runs, 200 steps each
tools/run_property_test.sh

# Run a focused operation suite (leaf DL ops / specific API ops)
tools/run_property_test.sh --op=DLInsert --num-runs=500 -v
tools/run_property_test.sh --op=OSFreeToHeap --num-runs=200 --steps=500 -v

# More runs
tools/run_property_test.sh --num-runs=2000 --steps=200 -v

# Reproduce a specific failure
tools/run_property_test.sh --seed=0xDEADBEEF --steps=500 -v

# Debug build
GC_HOST_DEBUG=1 tools/run_property_test.sh --seed=42
```

## Files

| File | Purpose |
|------|---------|
| `osalloc_oracle_offsets.h` | Oracle allocator using 32-bit offsets |
| `osalloc_property_test.c` | Seed-driven parity loop (alloc/free/check) |
| `tools/run_property_test.sh` | Build + run script |
| `src/sdk_port/os/OSAlloc.c` | Port under test |

## Ops

`--op=...` selects which suite to run:

- `DLAddFront`, `DLExtract`, `DLInsert`, `DLLookup` (leaf list operations)
- `OSAllocFromHeap` (alloc-only sequence + free-bytes parity)
- `OSFreeToHeap` (free-heavy sequence + coalescing/free-bytes parity)
- `OSDestroyHeap`, `OSAddToHeap` (API behavior checks)
- `full` (alloc/free mix + free-bytes parity after each step)
