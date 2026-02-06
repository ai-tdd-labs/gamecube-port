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
            // prev=0, next=0, size=new_size
            store_u32be(s + 0, 0);
            store_u32be(s + 4, 0);
            store_u32be(s + 8, new_size);

            // hd->free = start, hd->allocated = 0
            store_u32be(hd + 4, s);
            store_u32be(hd + 8, 0);

            return (int)heap;
        }
    }

    return -1;
}
