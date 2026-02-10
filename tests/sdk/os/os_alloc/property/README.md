# OSAlloc Property Testing (v2)

Property-based testing for the sdk_port's `OSAlloc.c` — proves that the
big-endian emulated heap allocator produces **exactly** the same results
as the original Nintendo GC SDK code from the MP4 decomp.

## Architecture

Single `-m32` binary containing both implementations:

| Component | Description |
|-----------|-------------|
| **Oracle** | Exact copy of decomp `OSAlloc.c` with `oracle_` prefix, running with native 32-bit pointers |
| **Port** | `sdk_port/os/OSAlloc.c` operating on emulated big-endian memory via `gc_mem` |

Both are driven with the same PRNG seed. After every operation, results are
compared (return values, pointer offsets, heap integrity via `OSCheckHeap`).

### Why `-m32`?

On the GameCube (PowerPC 32-bit):
- `sizeof(void*) == 4`, `sizeof(long) == 4`
- `sizeof(Cell) == 16` (prev + next + size + hd, each 4 bytes)
- `sizeof(HeapDesc) == 12` (size + free + allocated, each 4 bytes)

Compiling with `-m32` on the host gives identical struct sizes. This means:
- HeapDescs live **in the arena** (just like on the real GC)
- Pointer arithmetic is identical
- The oracle code is a **literal copy** of the decomp (no hacks needed)

v1 used 64-bit compilation, requiring HeapDescs in a separate static array
and custom offset arithmetic. v2 eliminates all of that.

## Oracle source

```
external/mp4-decomp/src/dolphin/os/OSAlloc.c
```

Adaptations from decomp to oracle (minimal):
1. Symbol rename: `DLAddFront` -> `oracle_DLAddFront`, etc.
2. Type aliases: `u8`->`uint8_t`, `u32`->`uint32_t`, `s32`->`int32_t`
3. Macro stubs: `ASSERTMSG1`->`((void)0)`, `ASSERTREPORT`->`if(!(cond)) return -1`
4. Missing `hd` field added to `struct Cell` (decomp uses `cell->hd` but omits it from declaration)
5. `cell->hd = hd` added in `OSAllocFromHeap` (decomp binary does this, source omits it)
6. `cell->hd = NULL` added in `OSFreeToHeap` (free cells have hd=NULL)

## Test levels

Tests are structured bottom-up, each level building on the previous:

### Level 0 — DL leaf functions
Individual linked-list primitives tested in isolation:
- `DLAddFront`: prepend cell to list
- `DLExtract`: remove cell from list
- `DLInsert`: address-sorted insert with coalescing
- `DLLookup`: search for cell in list

Each builds identical lists in both oracle (native pointers) and port
(emulated gc_mem), performs the operation, then walks both lists comparing
offsets and sizes.

### Level 1 — Individual API functions
Each API function tested with focused scenarios:
- `OSAllocFromHeap`: allocation-only sequences
- `OSFreeToHeap`: allocate then free in random order
- `OSCheckHeap`: alloc/free/check cycles
- `OSDestroyHeap`: destroy one heap, verify others unaffected
- `OSAddToHeap`: create heap with partial arena, add memory, continue allocating

### Level 2 — Full integration
Random mix of alloc/free operations across multiple heaps, with
`OSCheckHeap` verification after every operation. This is the default
test mode (`--op=full`).

## How to run

```bash
# Full integration (default), 500 seeds
tools/run_property_test.sh

# Full integration, 2000 seeds
tools/run_property_test.sh --num-runs=2000

# Leaf test: DLInsert with coalescing
tools/run_property_test.sh --op=DLInsert --num-runs=500

# Leaf test: DLExtract
tools/run_property_test.sh --op=DLExtract --num-runs=500

# API test: OSAllocFromHeap
tools/run_property_test.sh --op=OSAllocFromHeap --num-runs=500

# API test: OSFreeToHeap
tools/run_property_test.sh --op=OSFreeToHeap --num-runs=500

# API test: OSDestroyHeap
tools/run_property_test.sh --op=OSDestroyHeap --num-runs=500

# API test: OSAddToHeap
tools/run_property_test.sh --op=OSAddToHeap --num-runs=500

# Reproduce a specific failure
tools/run_property_test.sh --seed=0xDEADBEEF -v

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

The `-m32` compilation makes this especially strong: the oracle is a
**literal copy** of the decomp code, struct layouts are byte-for-byte
identical to the real GC, so there is no translation layer where bugs
could hide.

## Files

| File | Purpose |
|------|---------|
| `osalloc_oracle.h` | Oracle: exact decomp copy with `oracle_` prefix |
| `osalloc_property_test.c` | Test runner with all levels |
| `tools/run_property_test.sh` | Build + run script |
| `src/sdk_port/os/OSAlloc.c` | Port under test |
