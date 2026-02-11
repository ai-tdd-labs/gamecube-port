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

## Available `--op` values

| Op | Level | Description |
|----|-------|-------------|
| `DLAddFront` | 0 | Prepend to linked list |
| `DLExtract` | 0 | Remove from linked list |
| `DLInsert` | 0 | Sorted insert with coalescing |
| `DLLookup` | 0 | Search linked list |
| `OSAllocFromHeap` | 1 | Heap allocation |
| `OSFreeToHeap` | 1 | Heap deallocation |
| `OSCheckHeap` | 1 | Heap integrity check |
| `OSDestroyHeap` | 1 | Heap destruction |
| `OSAddToHeap` | 1 | Add memory to existing heap |
| `full` | 2 | Random mix of all operations (default) |

## Coverage

### Proven correct (oracle == port)

All core allocator functions are property-tested and produce identical
results between oracle and port across 2000+ random seeds:

| Function | Tested via |
|----------|-----------|
| `DLAddFront` | L0 leaf test + used internally by L1/L2 |
| `DLExtract` | L0 leaf test + used internally by L1/L2 |
| `DLInsert` | L0 leaf test (incl. coalescing) + used internally by L1/L2 |
| `DLLookup` | L0 leaf test |
| `OSInitAlloc` | L1 setup for every API test + L2 |
| `OSCreateHeap` | L1 setup for every API test + L2 |
| `OSAllocFromHeap` | L1 focused + L2 full integration |
| `OSFreeToHeap` | L1 focused + L2 full integration |
| `OSCheckHeap` | L1 focused + verified after every op in all levels |
| `OSSetCurrentHeap` | L1 setup (used in every test) |
| `OSDestroyHeap` | L1 focused |
| `OSAddToHeap` | L1 focused |

### Not yet tested

| Function | Reason |
|----------|--------|
| `OSAllocFixed` | Complex (multi-heap range splitting), separate porting traject |
| `OSReferentSize` | In oracle but no port-side test yet (needs `cell->hd` validation) |
| `OSVisitAllocated` | In oracle but no port-side test yet (iterator, low priority) |
| `DLOverlap` | In oracle, only used by `OSAllocFixed` |
| `DLSize` | In oracle, only used by `OSDumpHeap` (debug) |

### What the tests prove

For every seeded run, the test verifies:
- **Pointer offsets** — allocations return the same offset from heap base
- **Success/failure** — alloc returns non-NULL in oracle iff port does too
- **Heap integrity** — `OSCheckHeap` returns the same free-byte count
- **Linked list structure** — DL leaf tests compare full list walk (offset + size per node)
- **Coalescing** — `DLInsert` tests adjacent cells that must merge

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
