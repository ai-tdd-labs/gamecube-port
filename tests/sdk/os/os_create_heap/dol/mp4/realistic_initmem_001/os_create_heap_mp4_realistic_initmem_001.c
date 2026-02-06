#include <dolphin/types.h>

// Oracle snippets for OSInitAlloc and OSCreateHeap copied from MP4 SDK source.
#include "../../../../os_init_alloc/dol/oracle_os_init_alloc.h"
#include "oracle_os_create_heap.h"

static void write_result(u32 marker, u32 heap, u32 hd0_size, u32 hd0_free, u32 hd0_alloc) {
    volatile u32 *out = (volatile u32 *)0x80300000;
    out[0] = marker;
    out[1] = heap;
    out[2] = hd0_size;
    out[3] = hd0_free;
    out[4] = hd0_alloc;
}

int main(void) {
    void *arena_lo = (void *)0x80004000;
    void *arena_hi = (void *)0x81700000;

    void *new_lo = OSInitAlloc(arena_lo, arena_hi, 1);

    void *heap_start = (void *)((u32)new_lo);
    void *heap_end = (void *)0x81700000;
    int heap = OSCreateHeap(heap_start, heap_end);

    struct HeapDesc *hd0 = HeapArray ? &HeapArray[0] : (struct HeapDesc *)0;
    write_result(
        0xDEADBEEFu,
        (u32)heap,
        hd0 ? (u32)hd0->size : 0,
        hd0 ? (u32)hd0->free : 0,
        hd0 ? (u32)hd0->allocated : 0
    );

    while (1) {}
}
