#pragma once

#include <dolphin/types.h>

// Minimal Oracle for OSCreateHeap copied from:
//   decomp_mario_party_4/src/dolphin/os/OSAlloc.c
// Requires globals from oracle_os_init_alloc.h.

static inline u32 round_up32(u32 x) { return (x + 0x1Fu) & ~0x1Fu; }
static inline u32 round_down32(u32 x) { return x & ~0x1Fu; }

int OSCreateHeap(void *start, void *end) {
    int heap;
    struct HeapDesc *hd;

    start = (void *)round_up32((u32)start);
    end = (void *)round_down32((u32)end);

    for (heap = 0; heap < NumHeaps; heap++) {
        hd = &HeapArray[heap];
        if (hd->size < 0) {
            hd->size = (u32)end - (u32)start;
            // free list cell lives at start; in the SDK it's a struct Cell.
            // Layout is (prev,next,size) but OSCreateHeap only initializes the pointers+size.
            // We only need the resulting pointer + hd values for our RAM dump.
            ((u32 *)start)[0] = 0; // prev
            ((u32 *)start)[1] = 0; // next
            ((u32 *)start)[2] = hd->size;
            hd->free = (void *)start;
            hd->allocated = 0;
            return heap;
        }
    }
    return -1;
}
