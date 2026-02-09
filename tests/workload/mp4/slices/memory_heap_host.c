#include "game/memory.h"

// Host-only minimal heap allocator for MP4 workloads.
//
// MP4's process system allocates a per-process "heap" blob and then performs
// simple allocations within it. For host integration tests we don't need full
// free list correctness; we need:
// - deterministic allocations
// - a stable sentinel at heap[4] == 0xA5 for the stack-overlap check
// - ability to free the whole heap blob on process termination
//
// This is deliberately tiny and grows only when tests require it.

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef uint8_t u8;
typedef uint32_t u32;
typedef int32_t s32;

typedef struct HuHeapHdr {
    void *heap;
    u32 size;
    u32 off;
} HuHeapHdr;

static inline u32 round_up(u32 v, u32 a) { return (v + (a - 1u)) & ~(a - 1u); }

enum { HOST_HEAP_MAX = 256 };
static HuHeapHdr g_heaps[HOST_HEAP_MAX];
static u32 g_heaps_n;

static HuHeapHdr *heap_slot(void *heap) {
    for (u32 i = 0; i < g_heaps_n; i++) {
        if (g_heaps[i].heap == heap) return &g_heaps[i];
    }
    if (g_heaps_n >= HOST_HEAP_MAX) return NULL;
    HuHeapHdr *h = &g_heaps[g_heaps_n++];
    memset(h, 0, sizeof(*h));
    h->heap = heap;
    return h;
}

s32 HuMemMemoryAllocSizeGet(s32 size) {
    if (size <= 0) return 0;
    return (s32)round_up((u32)size, 32u);
}

void *HuMemDirectMalloc(s32 heap, s32 size) {
    (void)heap;
    if (size <= 0) return 0;
    void *p = calloc(1, (size_t)size);
    if (!p) return 0;
    // Stack-overlap guard used by process.c (expects 165 at heap[4]).
    ((u8 *)p)[4] = 0xA5;
    HuHeapHdr *h = heap_slot(p);
    if (h) {
        h->size = (u32)size;
        h->off = 0;
    }
    return p;
}

void HuMemDirectFree(void *ptr) {
    if (ptr) {
        for (u32 i = 0; i < g_heaps_n; i++) {
            if (g_heaps[i].heap == ptr) {
                g_heaps[i] = g_heaps[g_heaps_n - 1];
                g_heaps_n--;
                break;
            }
        }
    }
    free(ptr);
}

void HuMemHeapInit(void *heap, s32 size) {
    if (!heap || size <= 0) return;
    // Do NOT write into the heap header region: process.c expects heap[4] to stay
    // at 0xA5 for its stack-overlap check. Keep heap state out-of-band.
    HuHeapHdr *h = heap_slot(heap);
    if (!h) return;
    h->size = (u32)size;
    // Keep a deterministic start offset so allocations match across runs.
    h->off = round_up(0x40u, 32u);
    ((u8 *)heap)[4] = 0xA5;
}

void *HuMemMemoryAlloc(void *heap, s32 size, u32 retaddr) {
    (void)retaddr;
    if (!heap || size <= 0) return 0;
    HuHeapHdr *h = heap_slot(heap);
    if (!h) return 0;
    u32 need = round_up((u32)size, 32u);
    if (h->off + need > h->size) return 0;
    u8 *p = (u8 *)heap + h->off;
    h->off += need;
    // Fill allocated blocks with a recognizable pattern for debugging.
    memset(p, 0xCC, need);
    return p;
}

void HuMemMemoryFree(void *ptr, u32 retaddr) {
    (void)ptr;
    (void)retaddr;
    // Intentionally a no-op for host workloads.
}
