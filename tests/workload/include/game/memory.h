#pragma once

#include "dolphin/types.h"

// MP4 workload: minimal declarations needed to compile process.c.
// The workload scenarios do not call these functions yet.

enum {
    HEAP_SYSTEM = 0,
};

s32 HuMemMemoryAllocSizeGet(s32 size);
void *HuMemDirectMalloc(s32 heap, s32 size);
void HuMemDirectFree(void *ptr);
void HuMemHeapInit(void *heap, s32 size);
void *HuMemMemoryAlloc(void *heap, s32 size, u32 retaddr);
void HuMemMemoryFree(void *ptr, u32 retaddr);
