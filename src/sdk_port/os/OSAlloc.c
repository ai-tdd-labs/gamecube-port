#include <stdint.h>

#include "gc_mem.h"

// RAM-backed state (big-endian in MEM1) for dump comparability.
#include "../sdk_state.h"

// Port of the minimal heap initializer needed by early game init.
// Behavior is driven by deterministic expected-vs-actual tests.

// SDK global: __OSCurrHeap (volatile s32). We store it in RAM-backed state so
// host and PPC runs can be compared via RAM dumps.
//
// In DOL tests, the oracle defines actual storage for __OSCurrHeap in a TU
// (see tests/sdk/os/os_init_alloc/dol/oracle_os_init_alloc.h). Our sdk_port
// implementation does not rely on that; host harnesses should treat this as
// an accessor.
volatile int32_t __OSCurrHeap = -1;

// Internal state (not part of the SDK API). We keep these as link-visible
// globals so host scenarios can validate behavior without re-deriving values.
uint32_t __gc_osalloc_heap_array;
int32_t __gc_osalloc_num_heaps;
uint32_t __gc_osalloc_arena_start;
uint32_t __gc_osalloc_arena_end;

static inline int32_t state_load_i32(uint32_t off, int32_t fallback) {
    // If state page isn't mapped, fall back to the C globals for host-only.
    if (!gc_sdk_state_ptr(off, 4)) return fallback;
    return (int32_t)gc_sdk_state_load_u32be(off);
}

static inline uint32_t state_load_u32(uint32_t off, uint32_t fallback) {
    if (!gc_sdk_state_ptr(off, 4)) return fallback;
    return gc_sdk_state_load_u32be(off);
}

static inline void state_store_i32(uint32_t off, int32_t v) {
    if (!gc_sdk_state_ptr(off, 4)) return;
    gc_sdk_state_store_u32be(off, (uint32_t)v);
}

static inline void state_store_u32(uint32_t off, uint32_t v) {
    if (!gc_sdk_state_ptr(off, 4)) return;
    gc_sdk_state_store_u32be(off, v);
}

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

// ── DL helpers for emulated memory ──
//
// Cell layout in emulated RAM (16 bytes):
//   +0x00: prev  (u32 GC pointer)
//   +0x04: next  (u32 GC pointer)
//   +0x08: size  (u32)
//   +0x0C: hd    (u32 GC pointer to HeapDesc; 0 for free cells)
//
// These are non-static so property tests can call them directly for
// leaf-level DL testing.

// DLAddFront: prepend cell to list, return new head.
uint32_t port_DLAddFront(uint32_t list, uint32_t cell) {
    store_u32be(cell + 4, list); // cell->next = list
    store_u32be(cell + 0, 0);   // cell->prev = NULL
    if (list != 0) {
        store_u32be(list + 0, cell); // list->prev = cell
    }
    return cell;
}

// DLLookup: search for cell in list, return cell if found, 0 otherwise.
uint32_t port_DLLookup(uint32_t list, uint32_t cell) {
    for (; list != 0; list = load_u32be(list + 4)) {
        if (list == cell) {
            return list;
        }
    }
    return 0;
}

// DLExtract: remove cell from doubly-linked list, return new head.
uint32_t port_DLExtract(uint32_t list, uint32_t cell) {
    uint32_t cell_next = load_u32be(cell + 4);
    uint32_t cell_prev = load_u32be(cell + 0);

    if (cell_next != 0) {
        store_u32be(cell_next + 0, cell_prev); // next->prev = cell->prev
    }
    if (cell_prev == 0) {
        return cell_next; // removing head
    }
    store_u32be(cell_prev + 4, cell_next); // prev->next = cell->next
    return list;
}

// DLInsert: insert cell into address-sorted free list with coalescing.
uint32_t port_DLInsert(uint32_t list, uint32_t cell) {
    uint32_t prev = 0;
    uint32_t next = list;

    while (next != 0) {
        if (cell <= next) break;
        prev = next;
        next = load_u32be(next + 4); // next->next
    }

    store_u32be(cell + 4, next); // cell->next = next
    store_u32be(cell + 0, prev); // cell->prev = prev

    if (next != 0) {
        store_u32be(next + 0, cell); // next->prev = cell
        // Coalesce with next?
        uint32_t cell_size = load_u32be(cell + 8);
        if (cell + cell_size == next) {
            uint32_t next_size = load_u32be(next + 8);
            store_u32be(cell + 8, cell_size + next_size); // cell->size += next->size
            next = load_u32be(next + 4); // next = next->next
            store_u32be(cell + 4, next);  // cell->next = next
            if (next != 0) {
                store_u32be(next + 0, cell); // next->prev = cell
            }
        }
    }

    if (prev != 0) {
        store_u32be(prev + 4, cell); // prev->next = cell
        // Coalesce prev with cell?
        uint32_t prev_size = load_u32be(prev + 8);
        if (prev + prev_size == cell) {
            uint32_t cell_size = load_u32be(cell + 8);
            store_u32be(prev + 8, prev_size + cell_size); // prev->size += cell->size
            next = load_u32be(cell + 4);
            store_u32be(prev + 4, next); // prev->next = cell->next
            if (next != 0) {
                store_u32be(next + 0, prev); // next->prev = prev
            }
        }
        return list;
    }
    return cell;
}

void *OSInitAlloc(void *arenaStart, void *arenaEnd, int maxHeaps) {
    const uint32_t arena_lo = (uint32_t)(uintptr_t)arenaStart;
    const uint32_t arena_hi = (uint32_t)(uintptr_t)arenaEnd;
    const uint32_t array_size = (uint32_t)maxHeaps * (uint32_t)HEAPDESC_SIZE;

    __gc_osalloc_heap_array = arena_lo;
    __gc_osalloc_num_heaps = (int32_t)maxHeaps;
    state_store_u32(GC_SDK_OFF_OSALLOC_HEAP_ARRAY, arena_lo);
    state_store_i32(GC_SDK_OFF_OSALLOC_NUM_HEAPS, (int32_t)maxHeaps);

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
    state_store_i32(GC_SDK_OFF_OS_CURR_HEAP, -1);

    // arenaStart becomes HeapArray + arraySize, then rounded up to 32 bytes.
    uint32_t new_lo = arena_lo + array_size;
    new_lo = round_up(new_lo, ALIGNMENT);

    // ArenaEnd is rounded down to 32 bytes.
    __gc_osalloc_arena_start = new_lo;
    __gc_osalloc_arena_end = round_down(arena_hi, ALIGNMENT);
    state_store_u32(GC_SDK_OFF_OSALLOC_ARENA_START, __gc_osalloc_arena_start);
    state_store_u32(GC_SDK_OFF_OSALLOC_ARENA_END, __gc_osalloc_arena_end);

    return (void *)(uintptr_t)new_lo;
}

int OSCreateHeap(void *start, void *end) {
    uint32_t s = (uint32_t)(uintptr_t)start;
    uint32_t e = (uint32_t)(uintptr_t)end;

    s = round_up(s, ALIGNMENT);
    e = round_down(e, ALIGNMENT);

    const uint32_t heap_array = state_load_u32(GC_SDK_OFF_OSALLOC_HEAP_ARRAY, __gc_osalloc_heap_array);
    const int32_t num_heaps = state_load_i32(GC_SDK_OFF_OSALLOC_NUM_HEAPS, __gc_osalloc_num_heaps);

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
    int prev = (int)state_load_i32(GC_SDK_OFF_OS_CURR_HEAP, __OSCurrHeap);
    __OSCurrHeap = (int32_t)heap;
    state_store_i32(GC_SDK_OFF_OS_CURR_HEAP, (int32_t)heap);
    return prev;
}

void *OSAllocFromHeap(int heap, uint32_t size) {
    // Port of OSAllocFromHeap (OSAlloc.c).
    // We intentionally keep asserts out; expected-vs-actual tests drive correctness.
    if ((int32_t)size <= 0) return (void *)0;

    const uint32_t heap_array = state_load_u32(GC_SDK_OFF_OSALLOC_HEAP_ARRAY, __gc_osalloc_heap_array);
    const int32_t num_heaps = state_load_i32(GC_SDK_OFF_OSALLOC_NUM_HEAPS, __gc_osalloc_num_heaps);
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

    // Add allocated cell to front of allocated list (via DLAddFront).
    uint32_t alloc_head = load_u32be(hd + 8);
    alloc_head = port_DLAddFront(alloc_head, cell);
    store_u32be(cell + 12, hd);        // cell->hd = &HeapArray[heap] (emulated address)
    store_u32be(hd + 8, alloc_head);

    return (void *)(uintptr_t)(cell + 0x20u);
}

void *OSAlloc(uint32_t size) {
    // MP4 (and other games) calls OSAlloc(size) which allocates from the
    // current heap.
    int curr = (int)state_load_i32(GC_SDK_OFF_OS_CURR_HEAP, __OSCurrHeap);
    return OSAllocFromHeap(curr, size);
}

void OSFreeToHeap(int heap, void *ptr) {
    if (!ptr) return;

    const uint32_t heap_array = state_load_u32(GC_SDK_OFF_OSALLOC_HEAP_ARRAY, __gc_osalloc_heap_array);
    const int32_t num_heaps = state_load_i32(GC_SDK_OFF_OSALLOC_NUM_HEAPS, __gc_osalloc_num_heaps);
    if (heap_array == 0 || heap < 0 || heap >= num_heaps) return;

    const uint32_t hd = heap_array + (uint32_t)heap * (uint32_t)HEAPDESC_SIZE;
    if (load_i32be(hd + 0) < 0) return;

    uint32_t gc_ptr = (uint32_t)(uintptr_t)ptr;
    uint32_t cell = gc_ptr - 0x20u;

    // Remove from allocated list.
    uint32_t alloc_head = load_u32be(hd + 8);
    uint32_t new_alloc = port_DLExtract(alloc_head, cell);
    store_u32be(hd + 8, new_alloc);

    // Clear hd pointer (mark as free).
    store_u32be(cell + 12, 0);

    // Insert into free list (address-sorted, with coalescing).
    uint32_t free_head = load_u32be(hd + 4);
    uint32_t new_free = port_DLInsert(free_head, cell);
    store_u32be(hd + 4, new_free);
}

void OSDestroyHeap(int heap) {
    const uint32_t heap_array = state_load_u32(GC_SDK_OFF_OSALLOC_HEAP_ARRAY, __gc_osalloc_heap_array);
    const int32_t num_heaps = state_load_i32(GC_SDK_OFF_OSALLOC_NUM_HEAPS, __gc_osalloc_num_heaps);
    if (heap_array == 0 || heap < 0 || heap >= num_heaps) return;

    const uint32_t hd = heap_array + (uint32_t)heap * (uint32_t)HEAPDESC_SIZE;
    store_u32be(hd + 0, 0xFFFFFFFFu); // hd->size = -1
}

void OSAddToHeap(int heap, void *start, void *end) {
    const uint32_t heap_array = state_load_u32(GC_SDK_OFF_OSALLOC_HEAP_ARRAY, __gc_osalloc_heap_array);
    const int32_t num_heaps = state_load_i32(GC_SDK_OFF_OSALLOC_NUM_HEAPS, __gc_osalloc_num_heaps);
    if (heap_array == 0 || heap < 0 || heap >= num_heaps) return;

    const uint32_t hd = heap_array + (uint32_t)heap * (uint32_t)HEAPDESC_SIZE;
    if (load_i32be(hd + 0) < 0) return;

    uint32_t s = (uint32_t)(uintptr_t)start;
    uint32_t e = (uint32_t)(uintptr_t)end;

    s = round_up(s, ALIGNMENT);
    e = round_down(e, ALIGNMENT);

    uint32_t cell_size = e - s;
    // cell = start; cell->size = end - start
    store_u32be(s + 8, cell_size);
    store_u32be(s + 12, 0); // hd=0 (free cell)

    // hd->size += cell->size
    int32_t hd_size = load_i32be(hd + 0);
    store_u32be(hd + 0, (uint32_t)(hd_size + (int32_t)cell_size));

    // hd->free = DLInsert(hd->free, cell)
    uint32_t free_head = load_u32be(hd + 4);
    uint32_t new_free = port_DLInsert(free_head, s);
    store_u32be(hd + 4, new_free);
}

void OSFree(void *ptr) {
    int curr = (int)state_load_i32(GC_SDK_OFF_OS_CURR_HEAP, __OSCurrHeap);
    OSFreeToHeap(curr, ptr);
}

long OSCheckHeap(int heap) {
    const uint32_t heap_array = state_load_u32(GC_SDK_OFF_OSALLOC_HEAP_ARRAY, __gc_osalloc_heap_array);
    const int32_t num_heaps = state_load_i32(GC_SDK_OFF_OSALLOC_NUM_HEAPS, __gc_osalloc_num_heaps);
    const uint32_t arena_start = state_load_u32(GC_SDK_OFF_OSALLOC_ARENA_START, __gc_osalloc_arena_start);
    const uint32_t arena_end = state_load_u32(GC_SDK_OFF_OSALLOC_ARENA_END, __gc_osalloc_arena_end);

    if (heap_array == 0) return -1;
    if (heap < 0 || heap >= num_heaps) return -1;

    const uint32_t hd = heap_array + (uint32_t)heap * (uint32_t)HEAPDESC_SIZE;
    int32_t hd_size = load_i32be(hd + 0);
    if (hd_size < 0) return -1;

    long total = 0;
    long free_bytes = 0;

    // Walk allocated list.
    uint32_t alloc_head = load_u32be(hd + 8);
    if (alloc_head != 0 && load_u32be(alloc_head + 0) != 0) return -1; // head->prev must be NULL

    for (uint32_t cell = alloc_head; cell != 0; cell = load_u32be(cell + 4)) {
        if (cell < arena_start || cell >= arena_end) return -1;
        if ((cell & (ALIGNMENT - 1)) != 0) return -1;
        uint32_t cell_next = load_u32be(cell + 4);
        if (cell_next != 0 && load_u32be(cell_next + 0) != cell) return -1;
        int32_t cell_size = load_i32be(cell + 8);
        if (cell_size < 0x40) return -1;
        if ((cell_size & (ALIGNMENT - 1)) != 0) return -1;
        total += cell_size;
        if (total <= 0 || total > hd_size) return -1;
    }

    // Walk free list.
    uint32_t free_head = load_u32be(hd + 4);
    if (free_head != 0 && load_u32be(free_head + 0) != 0) return -1; // head->prev must be NULL

    for (uint32_t cell = free_head; cell != 0; cell = load_u32be(cell + 4)) {
        if (cell < arena_start || cell >= arena_end) return -1;
        if ((cell & (ALIGNMENT - 1)) != 0) return -1;
        uint32_t cell_next = load_u32be(cell + 4);
        if (cell_next != 0 && load_u32be(cell_next + 0) != cell) return -1;
        int32_t cell_size = load_i32be(cell + 8);
        if (cell_size < 0x40) return -1;
        if ((cell_size & (ALIGNMENT - 1)) != 0) return -1;
        if (cell_next != 0 && cell + (uint32_t)cell_size >= cell_next) return -1;
        total += cell_size;
        free_bytes += cell_size;
        free_bytes -= 0x20; // HEADERSIZE
        if (total <= 0 || total > hd_size) return -1;
    }

    if (total != hd_size) return -1;
    return free_bytes;
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
