#include <dolphin/types.h>
#include "../../oracle_os_init_alloc.h"

static void write_result(u32 marker, u32 ret, u32 heap_array, u32 num_heaps, u32 arena_start, u32 arena_end, u32 hd0_size, u32 hd0_free, u32 hd0_alloc, u32 curr_heap) {
    volatile u32 *out = (volatile u32 *)0x80300000;
    out[0] = marker;
    out[1] = ret;
    out[2] = heap_array;
    out[3] = num_heaps;
    out[4] = arena_start;
    out[5] = arena_end;
    out[6] = hd0_size;
    out[7] = hd0_free;
    out[8] = hd0_alloc;
    out[9] = curr_heap;
}

int main(void) {
    void *arena_lo = (void *)0x80002001;
    void *arena_hi = (void *)0x80004003;
    int max_heaps = 2;

    void *result = OSInitAlloc(arena_lo, arena_hi, max_heaps);

    // HeapArray/ArenaStart/ArenaEnd/NumHeaps are static in OSAlloc.c, but visible in this TU.
    struct HeapDesc *hd0 = HeapArray ? &HeapArray[0] : (struct HeapDesc *)0;

    write_result(
        0xDEADBEEFu,
        (u32)result,
        (u32)HeapArray,
        (u32)NumHeaps,
        (u32)ArenaStart,
        (u32)ArenaEnd,
        hd0 ? (u32)hd0->size : 0,
        hd0 ? (u32)hd0->free : 0,
        hd0 ? (u32)hd0->allocated : 0,
        (u32)__OSCurrHeap
    );

    while (1) {}
}
