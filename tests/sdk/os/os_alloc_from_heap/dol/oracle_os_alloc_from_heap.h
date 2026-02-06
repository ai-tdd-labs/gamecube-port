// Oracle snippet copied from:
//   decomp_mario_party_4/src/dolphin/os/OSAlloc.c
//
// This header expects oracle_os_init_alloc.h to have been included first,
// so the following statics exist in the TU:
//   - HeapArray, NumHeaps, ArenaStart, ArenaEnd, __OSCurrHeap
//
// We embed only what OSAllocFromHeap needs.

#ifndef ORACLE_OS_ALLOC_FROM_HEAP_H
#define ORACLE_OS_ALLOC_FROM_HEAP_H

#include <dolphin/types.h>

#define ALIGNMENT 32

// NOTE: The SDK uses a 0x20-byte header (HEADERSIZE). We model the fields
// that are used by the allocation algorithm.
struct Cell {
    struct Cell *prev;
    struct Cell *next;
    long size;
    void *hd; // HeapDesc* in the original SDK (used by validation).
};

static struct Cell *DLAddFront(struct Cell *list, struct Cell *cell) {
    cell->next = list;
    cell->prev = 0;
    if (list) {
        list->prev = cell;
    }
    return cell;
}

static struct Cell *DLExtract(struct Cell *list, struct Cell *cell) {
    if (cell->next) {
        cell->next->prev = cell->prev;
    }
    if (cell->prev == 0) {
        return cell->next;
    }
    cell->prev->next = cell->next;
    return list;
}

static inline void *OSAllocFromHeap(int heap, unsigned long size) {
    struct HeapDesc *hd;
    struct Cell *cell;
    struct Cell *newCell;
    long leftoverSize;

    if ((long)size <= 0) {
        return 0;
    }
    if (!HeapArray) {
        return 0;
    }
    if (heap < 0 || heap >= NumHeaps) {
        return 0;
    }
    if (HeapArray[heap].size < 0) {
        return 0;
    }

    hd = &HeapArray[heap];
    size += 0x20;
    size = (size + 0x1F) & 0xFFFFFFE0;

    for (cell = (struct Cell *)hd->free; cell != 0; cell = cell->next) {
        if ((long)size <= (long)cell->size) {
            break;
        }
    }
    if (cell == 0) {
        return 0;
    }

    leftoverSize = cell->size - (long)size;
    if (leftoverSize < 0x40) {
        hd->free = DLExtract((struct Cell *)hd->free, cell);
    } else {
        cell->size = (long)size;
        newCell = (struct Cell *)((u8 *)cell + size);
        newCell->size = leftoverSize;
        newCell->prev = cell->prev;
        newCell->next = cell->next;
        newCell->hd = 0;
        if (newCell->next) {
            newCell->next->prev = newCell;
        }
        if (newCell->prev) {
            newCell->prev->next = newCell;
        } else {
            hd->free = newCell;
        }
    }

    cell->hd = hd;
    hd->allocated = DLAddFront((struct Cell *)hd->allocated, cell);
    return (u8 *)cell + 0x20;
}

#endif

