/*
 * osalloc_oracle.h — Oracle implementation of OSAlloc for property testing.
 *
 * This is an EXACT copy of the MP4 decomp's OSAlloc.c
 *   (external/mp4-decomp/src/dolphin/os/OSAlloc.c)
 * with the following minimal changes:
 *
 *   1. Symbol rename:   all symbols get oracle_ prefix
 *   2. Type aliases:    u8->uint8_t, u32->uint32_t, s32->int32_t (GC types -> stdint)
 *   3. Macro stubs:     ASSERTMSG1->((void)0), ASSERTREPORT->if(!(cond)) return -1,
 *                        OSReport->((void)0), OFFSET/InRange -> oracle_ prefixed
 *   4. Missing field:   struct Cell gets `hd` pointer (decomp uses cell->hd but
 *                        declares Cell without it)
 *   5. cell->hd = hd:   set in OSAllocFromHeap after DLAddFront (decomp binary
 *                        does this, source omits it)
 *   6. cell->hd = NULL: set in OSFreeToHeap (free cells have hd=NULL)
 *
 * MUST be compiled with -m32 so that:
 *   sizeof(void*) == 4
 *   sizeof(long)  == 4
 *   sizeof(struct oracle_Cell)     == 16  (4+4+4+4 = GC exact)
 *   sizeof(struct oracle_HeapDesc) == 12  (4+4+4   = GC exact)
 *
 * With -m32, HeapDescs live IN the arena buffer (just like on the real GC),
 * pointer arithmetic is identical, and no hacks are needed.
 *
 * Oracle source: external/mp4-decomp/src/dolphin/os/OSAlloc.c
 */
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* ---- Stub macros (replace SDK asserts / debug output) ---- */
#define oracle_ASSERTMSG1(line, cond, msg) ((void)0)
#define oracle_ASSERTMSG(line, cond)       ((void)0)
#define oracle_OSReport(...)               ((void)0)
#define oracle_ASSERTREPORT(line, cond) \
    if (!(cond)) { return -1; }

/* ---- Utility macros (prefixed) ---- */
#define oracle_ALIGNMENT   32u
#define oracle_HEADERSIZE  32u
#define oracle_MINOBJSIZE  64u
#define oracle_OFFSET(addr, align) ((uint32_t)(uintptr_t)(addr) & ((align) - 1))
#define oracle_InRange(cell, arenaStart, arenaEnd) \
    (((uint32_t)(uintptr_t)(arenaStart) <= (uint32_t)(uintptr_t)(cell)) && \
     ((uint32_t)(uintptr_t)(cell) < (uint32_t)(uintptr_t)(arenaEnd)))

/* ---- Structs (match GC layout exactly with -m32) ---- */

struct oracle_Cell {
    struct oracle_Cell     *prev;
    struct oracle_Cell     *next;
    long                    size;
    struct oracle_HeapDesc *hd;   /* missing in decomp declaration, but used */
};

struct oracle_HeapDesc {
    long                    size;
    struct oracle_Cell     *free;
    struct oracle_Cell     *allocated;
};

/* ---- Globals ---- */
static volatile int oracle___OSCurrHeap = -1;

static struct oracle_HeapDesc *oracle_HeapArray;
static int    oracle_NumHeaps;
static void  *oracle_ArenaStart;
static void  *oracle_ArenaEnd;

/* ---- DL helpers (exact copy of decomp, prefixed) ---- */

static struct oracle_Cell *oracle_DLAddFront(struct oracle_Cell *list,
                                             struct oracle_Cell *cell)
{
    cell->next = list;
    cell->prev = 0;
    if (list) {
        list->prev = cell;
    }
    return cell;
}

static struct oracle_Cell *oracle_DLLookup(struct oracle_Cell *list,
                                           struct oracle_Cell *cell)
{
    for (; list; list = list->next) {
        if (list == cell) {
            return list;
        }
    }
    return NULL;
}

static struct oracle_Cell *oracle_DLExtract(struct oracle_Cell *list,
                                            struct oracle_Cell *cell)
{
    if (cell->next) {
        cell->next->prev = cell->prev;
    }
    if (cell->prev == NULL) {
        return cell->next;
    }
    cell->prev->next = cell->next;
    return list;
}

static struct oracle_Cell *oracle_DLInsert(struct oracle_Cell *list,
                                           struct oracle_Cell *cell)
{
    struct oracle_Cell *prev;
    struct oracle_Cell *next;

    for (next = list, prev = NULL; next != 0; prev = next, next = next->next) {
        if (cell <= next) {
            break;
        }
    }

    cell->next = next;
    cell->prev = prev;
    if (next) {
        next->prev = cell;
        if ((uint8_t *)cell + cell->size == (uint8_t *)next) {
            cell->size += next->size;
            next = next->next;
            cell->next = next;
            if (next) {
                next->prev = cell;
            }
        }
    }
    if (prev) {
        prev->next = cell;
        if ((uint8_t *)prev + prev->size == (uint8_t *)cell) {
            prev->size += cell->size;
            prev->next = next;
            if (next) {
                next->prev = prev;
            }
        }
        return list;
    }
    return cell;
}

static int oracle_DLOverlap(struct oracle_Cell *list, void *start, void *end)
{
    struct oracle_Cell *cell = list;

    while (cell) {
        if (((start <= (void *)cell) && ((void *)cell < end)) ||
            ((start < (void *)((uint8_t *)cell + cell->size)) &&
             ((void *)((uint8_t *)cell + cell->size) <= end))) {
            return 1;
        }
        cell = cell->next;
    }
    return 0;
}

static long oracle_DLSize(struct oracle_Cell *list)
{
    struct oracle_Cell *cell;
    long size;

    size = 0;
    cell = list;

    while (cell) {
        size += cell->size;
        cell = cell->next;
    }

    return size;
}

/* ---- API functions ---- */

static void *oracle_OSInitAlloc(void *arenaStart, void *arenaEnd, int maxHeaps)
{
    unsigned long arraySize;
    int i;
    struct oracle_HeapDesc *hd;

    oracle_ASSERTMSG1(0x283, maxHeaps > 0, "OSInitAlloc(): invalid number of heaps.");
    oracle_ASSERTMSG1(0x285, (uint32_t)(uintptr_t)arenaStart < (uint32_t)(uintptr_t)arenaEnd,
                      "OSInitAlloc(): invalid range.");

    arraySize = maxHeaps * sizeof(struct oracle_HeapDesc);
    oracle_HeapArray = arenaStart;
    oracle_NumHeaps = maxHeaps;

    for (i = 0; i < oracle_NumHeaps; i++) {
        hd = &oracle_HeapArray[i];
        hd->size = -1;
        hd->free = hd->allocated = 0;
    }
    oracle___OSCurrHeap = -1;

    arenaStart = (void *)((uint32_t)((char *)oracle_HeapArray + arraySize));
    arenaStart = (void *)(((uint32_t)(uintptr_t)arenaStart + 0x1F) & 0xFFFFFFE0);
    oracle_ArenaStart = arenaStart;
    oracle_ArenaEnd = (void *)((uint32_t)(uintptr_t)arenaEnd & 0xFFFFFFE0);

    oracle_ASSERTMSG1(0x2A4,
        ((uint32_t)(uintptr_t)oracle_ArenaEnd - (uint32_t)(uintptr_t)oracle_ArenaStart) >= 0x40U,
        "OSInitAlloc(): too small range.");

    return arenaStart;
}

static int oracle_OSCreateHeap(void *start, void *end)
{
    int heap;
    struct oracle_HeapDesc *hd;
    struct oracle_Cell *cell;

    oracle_ASSERTMSG1(0x2BD, oracle_HeapArray, "OSCreateHeap(): heap is not initialized.");
    oracle_ASSERTMSG1(0x2BE, (uint32_t)(uintptr_t)start < (uint32_t)(uintptr_t)end,
                      "OSCreateHeap(): invalid range.");

    start = (void *)(((uint32_t)(uintptr_t)start + 0x1FU) & ~((32) - 1));
    end   = (void *)(((uint32_t)(uintptr_t)end)           & ~((32) - 1));

    oracle_ASSERTMSG1(0x2C1, (uint32_t)(uintptr_t)start < (uint32_t)(uintptr_t)end,
                      "OSCreateHeap(): invalid range.");
    oracle_ASSERTMSG1(0x2C5,
        ((uint32_t)(uintptr_t)end - (uint32_t)(uintptr_t)start) >= 0x40U,
        "OSCreateHeap(): too small range.");

    for (heap = 0; heap < oracle_NumHeaps; heap++) {
        hd = &oracle_HeapArray[heap];
        if (hd->size < 0) {
            hd->size = (uint32_t)(uintptr_t)end - (uint32_t)(uintptr_t)start;
            cell = start;
            cell->prev = 0;
            cell->next = 0;
            cell->size = hd->size;
            cell->hd = 0;
            hd->free = cell;
            hd->allocated = 0;
            return heap;
        }
    }
    return -1;
}

static void oracle_OSDestroyHeap(int heap)
{
    struct oracle_HeapDesc *hd;

    oracle_ASSERTMSG1(0x30A, oracle_HeapArray, "OSDestroyHeap(): heap is not initialized.");
    oracle_ASSERTMSG1(0x30B, (heap >= 0) && (heap < oracle_NumHeaps),
                      "OSDestroyHeap(): invalid heap handle.");
    oracle_ASSERTMSG1(0x30C, oracle_HeapArray[heap].size >= 0,
                      "OSDestroyHeap(): invalid heap handle.");

    hd = &oracle_HeapArray[heap];
    hd->size = -1;
}

static void oracle_OSAddToHeap(int heap, void *start, void *end)
{
    struct oracle_HeapDesc *hd;
    struct oracle_Cell *cell;

    oracle_ASSERTMSG1(0x339, oracle_HeapArray, "OSAddToHeap(): heap is not initialized.");
    oracle_ASSERTMSG1(0x33A, (heap >= 0) && (heap < oracle_NumHeaps),
                      "OSAddToHeap(): invalid heap handle.");
    oracle_ASSERTMSG1(0x33B, oracle_HeapArray[heap].size >= 0,
                      "OSAddToHeap(): invalid heap handle.");

    hd = &oracle_HeapArray[heap];

    oracle_ASSERTMSG1(0x33F, (uint32_t)(uintptr_t)start < (uint32_t)(uintptr_t)end,
                      "OSAddToHeap(): invalid range.");

    start = (void *)(((uint32_t)(uintptr_t)start + 0x1F) & ~((32) - 1));
    end   = (void *)(((uint32_t)(uintptr_t)end)           & ~((32) - 1));

    oracle_ASSERTMSG1(0x343,
        ((uint32_t)(uintptr_t)end - (uint32_t)(uintptr_t)start) >= 0x40U,
        "OSAddToHeap(): too small range.");

    cell = (struct oracle_Cell *)start;
    cell->size = ((char *)end - (char *)start);
    cell->hd = 0;
    hd->size += cell->size;
    hd->free = oracle_DLInsert(hd->free, cell);
}

static void *oracle_OSAllocFromHeap(int heap, unsigned long size)
{
    struct oracle_HeapDesc *hd;
    struct oracle_Cell *cell;
    struct oracle_Cell *newCell;
    long leftoverSize;

    oracle_ASSERTMSG1(0x14D, oracle_HeapArray, "OSAllocFromHeap(): heap is not initialized.");
    oracle_ASSERTMSG1(0x14E, (signed long)size > 0, "OSAllocFromHeap(): invalid size.");
    oracle_ASSERTMSG1(0x14F, heap >= 0 && heap < oracle_NumHeaps,
                      "OSAllocFromHeap(): invalid heap handle.");
    oracle_ASSERTMSG1(0x150, oracle_HeapArray[heap].size >= 0,
                      "OSAllocFromHeap(): invalid heap handle.");

    hd = &oracle_HeapArray[heap];
    size += 0x20;
    size = (size + 0x1F) & 0xFFFFFFE0;

    for (cell = hd->free; cell != NULL; cell = cell->next) {
        if ((signed long)size <= (signed long)cell->size) {
            break;
        }
    }

    if (cell == NULL) {
        return NULL;
    }

    oracle_ASSERTMSG1(0x168, !((int32_t)(uintptr_t)cell & 0x1F),
                      "OSAllocFromHeap(): heap is broken.");
    oracle_ASSERTMSG1(0x169, cell->hd == NULL,
                      "OSAllocFromHeap(): heap is broken.");

    leftoverSize = cell->size - (long)size;
    if (leftoverSize < (long)0x40U) {
        hd->free = oracle_DLExtract(hd->free, cell);
    }
    else {
        cell->size = (long)size;
        newCell = (void *)((uint8_t *)cell + size);
        newCell->size = leftoverSize;
        newCell->prev = cell->prev;
        newCell->next = cell->next;
        newCell->hd = 0;
        if (newCell->next != NULL) {
            newCell->next->prev = newCell;
        }
        if (newCell->prev != NULL) {
            newCell->prev->next = newCell;
        }
        else {
            oracle_ASSERTMSG1(0x186, hd->free == cell,
                              "OSAllocFromHeap(): heap is broken.");
            hd->free = newCell;
        }
    }

    hd->allocated = oracle_DLAddFront(hd->allocated, cell);
    cell->hd = hd;   /* decomp binary sets this; source omits it */
    return (uint8_t *)cell + 0x20;
}

static void oracle_OSFreeToHeap(int heap, void *ptr)
{
    struct oracle_HeapDesc *hd;
    struct oracle_Cell *cell;

    oracle_ASSERTMSG1(0x23D, oracle_HeapArray, "OSFreeToHeap(): heap is not initialized.");

    if (!ptr) return;
    if (heap < 0 || heap >= oracle_NumHeaps) return;
    if (oracle_HeapArray[heap].size < 0) return;

    cell = (void *)((uint32_t)(uintptr_t)ptr - 0x20);
    hd = &oracle_HeapArray[heap];

    hd->allocated = oracle_DLExtract(hd->allocated, cell);
    cell->hd = NULL;   /* mark as free */
    hd->free = oracle_DLInsert(hd->free, cell);
}

static int oracle_OSSetCurrentHeap(int heap)
{
    int prev;

    oracle_ASSERTMSG1(0x267, oracle_HeapArray, "OSSetCurrentHeap(): heap is not initialized.");

    prev = oracle___OSCurrHeap;
    oracle___OSCurrHeap = heap;
    return prev;
}

static long oracle_OSCheckHeap(int heap)
{
    struct oracle_HeapDesc *hd;
    struct oracle_Cell *cell;
    long total = 0;
    long free = 0;

    oracle_ASSERTREPORT(0x37D, oracle_HeapArray);
    oracle_ASSERTREPORT(0x37E, 0 <= heap && heap < oracle_NumHeaps);
    hd = &oracle_HeapArray[heap];
    oracle_ASSERTREPORT(0x381, 0 <= hd->size);

    oracle_ASSERTREPORT(0x383, hd->allocated == NULL || hd->allocated->prev == NULL);

    for (cell = hd->allocated; cell; cell = cell->next) {
        oracle_ASSERTREPORT(0x386, oracle_InRange(cell, oracle_ArenaStart, oracle_ArenaEnd));
        oracle_ASSERTREPORT(0x387, oracle_OFFSET(cell, oracle_ALIGNMENT) == 0);
        oracle_ASSERTREPORT(0x388, cell->next == NULL || cell->next->prev == cell);
        oracle_ASSERTREPORT(0x389, (long)oracle_MINOBJSIZE <= cell->size);
        oracle_ASSERTREPORT(0x38A, oracle_OFFSET(cell->size, oracle_ALIGNMENT) == 0);
        total += cell->size;
        oracle_ASSERTREPORT(0x38D, 0 < total && total <= hd->size);
    }

    oracle_ASSERTREPORT(0x395, hd->free == NULL || hd->free->prev == NULL);

    for (cell = hd->free; cell; cell = cell->next) {
        oracle_ASSERTREPORT(0x398, oracle_InRange(cell, oracle_ArenaStart, oracle_ArenaEnd));
        oracle_ASSERTREPORT(0x399, oracle_OFFSET(cell, oracle_ALIGNMENT) == 0);
        oracle_ASSERTREPORT(0x39A, cell->next == NULL || cell->next->prev == cell);
        oracle_ASSERTREPORT(0x39B, (long)oracle_MINOBJSIZE <= cell->size);
        oracle_ASSERTREPORT(0x39C, oracle_OFFSET(cell->size, oracle_ALIGNMENT) == 0);
        oracle_ASSERTREPORT(0x39D, cell->next == NULL ||
            (char *)cell + cell->size < (char *)cell->next);
        total += cell->size;
        free = (cell->size + free);
        free -= oracle_HEADERSIZE;
        oracle_ASSERTREPORT(0x3A1, 0 < total && total <= hd->size);
    }
    oracle_ASSERTREPORT(0x3A8, total == hd->size);
    return free;
}

static unsigned long oracle_OSReferentSize(void *ptr)
{
    struct oracle_Cell *cell;

    oracle_ASSERTMSG1(0x3BB, oracle_HeapArray, "OSReferentSize(): heap is not initialized.");

    cell = (void *)((uint32_t)(uintptr_t)ptr - oracle_HEADERSIZE);
    return (long)((uint32_t)(uintptr_t)cell->size - oracle_HEADERSIZE);
}

static void oracle_OSVisitAllocated(void (*visitor)(void *, unsigned long))
{
    unsigned long heap;
    struct oracle_Cell *cell;

    for (heap = 0; heap < (unsigned long)oracle_NumHeaps; heap++) {
        if (oracle_HeapArray[heap].size >= 0) {
            for (cell = oracle_HeapArray[heap].allocated; cell; cell = cell->next) {
                visitor((char *)cell + oracle_HEADERSIZE, cell->size);
            }
        }
    }
}

/* ---- Reset helper for test harness ---- */
static void oracle_reset(void *arena_buf, size_t arena_size)
{
    memset(arena_buf, 0, arena_size);
    oracle_HeapArray = NULL;
    oracle_NumHeaps = 0;
    oracle_ArenaStart = NULL;
    oracle_ArenaEnd = NULL;
    oracle___OSCurrHeap = -1;
}
