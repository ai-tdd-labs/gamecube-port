#pragma once

#include <dolphin/types.h>

// Oracle snippet for OSInitAlloc copied from:
//   decomp_mario_party_4/src/dolphin/os/OSAlloc.c
//
// We embed only the required types + function to avoid pulling in headers that
// depend on Metrowerks-specific inline asm.

struct HeapDesc {
    s32 size;
    void *free;
    void *allocated;
};

volatile s32 __OSCurrHeap = -1;

static struct HeapDesc *HeapArray;
static int NumHeaps;
static void *ArenaStart;
static void *ArenaEnd;

void *OSInitAlloc(void *arenaStart, void *arenaEnd, int maxHeaps) {
    unsigned long arraySize;
    int i;
    struct HeapDesc *hd;

    arraySize = (unsigned long)maxHeaps * sizeof(struct HeapDesc);
    HeapArray = (struct HeapDesc *)arenaStart;
    NumHeaps = maxHeaps;

    for (i = 0; i < NumHeaps; i++) {
        hd = &HeapArray[i];
        hd->size = -1;
        hd->free = 0;
        hd->allocated = 0;
    }
    __OSCurrHeap = -1;
    arenaStart = (void *)((u32)((char *)HeapArray + arraySize));
    arenaStart = (void *)(((u32)arenaStart + 0x1F) & 0xFFFFFFE0);
    ArenaStart = arenaStart;
    ArenaEnd = (void *)((u32)arenaEnd & 0xFFFFFFE0);
    return arenaStart;
}

