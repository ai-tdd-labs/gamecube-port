#include <stdint.h>

#include "gc_mem.h"

// Port of the minimal heap initializer needed by early game init.
// Behavior is driven by deterministic expected-vs-actual tests.

// Matches the SDK global (exposed in the header as extern volatile).
volatile int32_t __OSCurrHeap = -1;

// Internal state (not part of the SDK API). We keep these as link-visible
// globals so host scenarios can validate behavior without re-deriving values.
uint32_t __gc_osalloc_heap_array;
int32_t __gc_osalloc_num_heaps;
uint32_t __gc_osalloc_arena_start;
uint32_t __gc_osalloc_arena_end;

static inline int32_t load_i32be(uint32_t addr) {
    uint8_t *p = gc_mem_ptr(addr, 4);
    if (!p) return 0;
    return (int32_t)(((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3]);
}

static inline uint32_t load_u32be(uint32_t addr) {
    return (uint32_t)load_i32be(addr);
}

static inline uint32_t round_up(uint32_t x, uint32_t a) {
    return (x + (a - 1)) & ~(a - 1);
}

static inline uint32_t round_down(uint32_t x, uint32_t a) {
    return x & ~(a - 1);
}

static inline void store_u32be(uint32_t addr, uint32_t v) {
    uint8_t *p = gc_mem_ptr(addr, 4);
    if (!p) return;
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)(v >> 0);
}

// OSAlloc.c uses:
//   struct HeapDesc { long size; Cell *free; Cell *allocated; };
// which is 12 bytes on GC (3x 32-bit fields).
enum { HEAPDESC_SIZE = 12, ALIGNMENT = 32 };

void *OSInitAlloc(void *arenaStart, void *arenaEnd, int maxHeaps) {
    const uint32_t arena_lo = (uint32_t)(uintptr_t)arenaStart;
    const uint32_t arena_hi = (uint32_t)(uintptr_t)arenaEnd;
    const uint32_t array_size = (uint32_t)maxHeaps * (uint32_t)HEAPDESC_SIZE;

    __gc_osalloc_heap_array = arena_lo;
    __gc_osalloc_num_heaps = (int32_t)maxHeaps;

    // HeapArray lives at arena_lo (in emulated RAM).
    // Initialize every HeapDesc:
    //   size = -1, free = 0, allocated = 0
    for (int i = 0; i < maxHeaps; i++) {
        uint32_t hd = arena_lo + (uint32_t)i * (uint32_t)HEAPDESC_SIZE;
        store_u32be(hd + 0, 0xFFFFFFFFu);
        store_u32be(hd + 4, 0);
        store_u32be(hd + 8, 0);
    }

    __OSCurrHeap = -1;

    // arenaStart becomes HeapArray + arraySize, then rounded up to 32 bytes.
    uint32_t new_lo = arena_lo + array_size;
    new_lo = round_up(new_lo, ALIGNMENT);

    // ArenaEnd is rounded down to 32 bytes.
    __gc_osalloc_arena_start = new_lo;
    __gc_osalloc_arena_end = round_down(arena_hi, ALIGNMENT);

    return (void *)(uintptr_t)new_lo;
}

int OSCreateHeap(void *start, void *end) {
    uint32_t s = (uint32_t)(uintptr_t)start;
    uint32_t e = (uint32_t)(uintptr_t)end;

    s = round_up(s, ALIGNMENT);
    e = round_down(e, ALIGNMENT);

    const uint32_t heap_array = __gc_osalloc_heap_array;
    const int32_t num_heaps = __gc_osalloc_num_heaps;

    for (int32_t heap = 0; heap < num_heaps; heap++) {
        uint32_t hd = heap_array + (uint32_t)heap * (uint32_t)HEAPDESC_SIZE;
        int32_t hd_size = load_i32be(hd + 0);
        if (hd_size < 0) {
            uint32_t new_size = e - s;

            // hd->size
            store_u32be(hd + 0, new_size);

            // Write the initial free list cell at start:
            // prev=0, next=0, size=new_size, hd=0 (free cell)
            store_u32be(s + 0, 0);
            store_u32be(s + 4, 0);
            store_u32be(s + 8, new_size);
            store_u32be(s + 12, 0);

            // hd->free = start, hd->allocated = 0
            store_u32be(hd + 4, s);
            store_u32be(hd + 8, 0);

            return (int)heap;
        }
    }

    return -1;
}

int OSSetCurrentHeap(int heap) {
    // SDK contract: set __OSCurrHeap and return previous.
    // Asserts in the original SDK are intentionally omitted; tests drive correctness.
    int prev = (int)__OSCurrHeap;
    __OSCurrHeap = (int32_t)heap;
    return prev;
}

void *OSAllocFromHeap(int heap, uint32_t size) {
    // Port of OSAllocFromHeap (OSAlloc.c).
    // We intentionally keep asserts out; expected-vs-actual tests drive correctness.
    if ((int32_t)size <= 0) return (void *)0;

    const uint32_t heap_array = __gc_osalloc_heap_array;
    const int32_t num_heaps = __gc_osalloc_num_heaps;
    if (heap_array == 0 || heap < 0 || heap >= num_heaps) return (void *)0;

    const uint32_t hd = heap_array + (uint32_t)heap * (uint32_t)HEAPDESC_SIZE;
    const int32_t hd_size = load_i32be(hd + 0);
    if (hd_size < 0) return (void *)0;

    // size includes 0x20 header, then rounded up to 32 bytes.
    uint32_t req = size + 0x20u;
    req = round_up(req, ALIGNMENT);

    uint32_t cell = load_u32be(hd + 4); // hd->free
    while (cell != 0) {
        uint32_t cell_size = load_u32be(cell + 8);
        if ((int32_t)req <= (int32_t)cell_size) break;
        cell = load_u32be(cell + 4); // next
    }
    if (cell == 0) return (void *)0;

    const uint32_t cell_prev = load_u32be(cell + 0);
    const uint32_t cell_next = load_u32be(cell + 4);
    const uint32_t cell_size = load_u32be(cell + 8);

    uint32_t leftover = cell_size - req;
    if (leftover < 0x40u) {
        // Extract from free list.
        if (cell_next != 0) {
            store_u32be(cell_next + 0, cell_prev);
        }
        if (cell_prev == 0) {
            // Removing head.
            store_u32be(hd + 4, cell_next);
        } else {
            store_u32be(cell_prev + 4, cell_next);
        }
    } else {
        // Split: keep 'cell' as allocated chunk, create 'newCell' as remaining free chunk.
        store_u32be(cell + 8, req);

        uint32_t new_cell = cell + req;
        store_u32be(new_cell + 8, leftover);
        store_u32be(new_cell + 0, cell_prev);
        store_u32be(new_cell + 4, cell_next);
        store_u32be(new_cell + 12, 0); // free cell has hd=NULL

        if (cell_next != 0) {
            store_u32be(cell_next + 0, new_cell);
        }
        if (cell_prev != 0) {
            store_u32be(cell_prev + 4, new_cell);
        } else {
            // Replacing head.
            store_u32be(hd + 4, new_cell);
        }
    }

    // Add allocated cell to front of allocated list.
    uint32_t alloc_head = load_u32be(hd + 8);
    store_u32be(cell + 4, alloc_head); // next
    store_u32be(cell + 0, 0);          // prev
    store_u32be(cell + 12, hd);        // cell->hd = &HeapArray[heap] (emulated address)
    if (alloc_head != 0) {
        store_u32be(alloc_head + 0, cell);
    }
    store_u32be(hd + 8, cell);

    return (void *)(uintptr_t)(cell + 0x20u);
}

void *OSAlloc(uint32_t size) {
    // MP4 (and other games) calls OSAlloc(size) which allocates from the
    // current heap.
    return OSAllocFromHeap((int)__OSCurrHeap, size);
}

// Minimal extras used by some init paths and debug helpers.
uint32_t gc_os_alloc_fixed_calls;
uint32_t gc_os_dump_heap_calls;
uint32_t gc_os_alloc_fixed_last_size;

void *OSAllocFixed(uint32_t size) {
    gc_os_alloc_fixed_calls++;
    gc_os_alloc_fixed_last_size = size;
    return OSAlloc(size);
}

void OSDumpHeap(void) {
    gc_os_dump_heap_calls++;
}
