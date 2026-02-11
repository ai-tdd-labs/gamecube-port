/*
 * osalloc_oracle_offsets.h — Portable oracle implementation of OSAlloc.
 *
 * This is derived from the MP4 decomp's OSAlloc.c but rewritten to use 32-bit
 * *offset pointers* (u32) instead of native C pointers.
 *
 * Rationale:
 * - The original GameCube SDK is 32-bit (PPC). `sizeof(void*) == 4`.
 * - Modern macOS/Windows typically build 64-bit binaries (pointers are 8 bytes).
 * - Compiling with `-m32` is not reliably supported on macOS/Apple Silicon.
 *
 * Using offsets preserves the 32-bit pointer arithmetic and struct sizes in a
 * way that is fully portable across 64-bit hosts.
 *
 * Oracle source (behavioral reference):
 * - external/mp4-decomp/src/dolphin/os/OSAlloc.c
 *
 * Note:
 * - This oracle is intended for *semantic parity* (return offsets, free bytes,
 *   list structure, coalescing). It does not attempt to reproduce raw byte
 *   layouts under big-endian memory dumps; that's covered by Dolphin DOL tests.
 */
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <string.h>

typedef uint32_t o32; /* 0 == NULL */

enum { ORACLE_ALIGNMENT = 32u, ORACLE_HEADERSIZE = 32u, ORACLE_MINOBJSIZE = 64u };

struct oracle_Cell {
    o32 prev;
    o32 next;
    int32_t size;
    o32 hd; /* HeapDesc offset when allocated; 0 when free */
};

struct oracle_HeapDesc {
    int32_t size;
    o32 free;
    o32 allocated;
};

_Static_assert(sizeof(struct oracle_Cell) == 16, "oracle_Cell must be 16 bytes");
_Static_assert(sizeof(struct oracle_HeapDesc) == 12, "oracle_HeapDesc must be 12 bytes");

static uint8_t *oracle_base;
static uint32_t oracle_len;

static volatile int32_t oracle___OSCurrHeap = -1;
static o32 oracle_HeapArray; /* offset of HeapDesc array in oracle arena */
static int32_t oracle_NumHeaps;
static o32 oracle_ArenaStart;
static o32 oracle_ArenaEnd;

static inline uint32_t oracle_round_up(uint32_t x, uint32_t a) { return (x + (a - 1)) & ~(a - 1); }
static inline uint32_t oracle_round_down(uint32_t x, uint32_t a) { return x & ~(a - 1); }

static inline struct oracle_Cell *OCELL(o32 off) {
    if (off == 0) return NULL;
    if (off + sizeof(struct oracle_Cell) > oracle_len) return NULL;
    return (struct oracle_Cell *)(oracle_base + off);
}

static inline struct oracle_HeapDesc *OHD(o32 off) {
    if (off == 0) return NULL;
    if (off + sizeof(struct oracle_HeapDesc) > oracle_len) return NULL;
    return (struct oracle_HeapDesc *)(oracle_base + off);
}

static inline int oracle_InRange(o32 cell, o32 start, o32 end) {
    return (start <= cell) && (cell < end);
}

static inline void oracle_reset(uint8_t *base, uint32_t len) {
    oracle_base = base;
    oracle_len = len;
    oracle___OSCurrHeap = -1;
    oracle_HeapArray = 0;
    oracle_NumHeaps = 0;
    oracle_ArenaStart = 0;
    oracle_ArenaEnd = 0;
    /* leave contents unspecified; callers will init as needed */
}

/* ── DL helpers (offset-pointer version) ── */

static o32 oracle_DLAddFront(o32 list, o32 cell) {
    struct oracle_Cell *c = OCELL(cell);
    if (!c) return 0;
    c->next = list;
    c->prev = 0;
    if (list) {
        struct oracle_Cell *l = OCELL(list);
        if (l) l->prev = cell;
    }
    return cell;
}

static o32 oracle_DLLookup(o32 list, o32 cell) {
    for (o32 it = list; it != 0; ) {
        if (it == cell) return it;
        struct oracle_Cell *c = OCELL(it);
        if (!c) return 0;
        it = c->next;
    }
    return 0;
}

static o32 oracle_DLExtract(o32 list, o32 cell) {
    struct oracle_Cell *c = OCELL(cell);
    if (!c) return list;

    if (c->next) {
        struct oracle_Cell *n = OCELL(c->next);
        if (n) n->prev = c->prev;
    }
    if (c->prev == 0) {
        return c->next;
    }
    struct oracle_Cell *p = OCELL(c->prev);
    if (p) p->next = c->next;
    return list;
}

static o32 oracle_DLInsert(o32 list, o32 cell) {
    o32 prev = 0;
    o32 next = list;

    while (next != 0) {
        if (cell <= next) break;
        prev = next;
        struct oracle_Cell *n = OCELL(next);
        if (!n) break;
        next = n->next;
    }

    struct oracle_Cell *c = OCELL(cell);
    if (!c) return list;
    c->next = next;
    c->prev = prev;

    if (next) {
        struct oracle_Cell *n = OCELL(next);
        if (n) {
            n->prev = cell;
            if ((uint32_t)(cell + (o32)c->size) == next) {
                c->size += n->size;
                next = n->next;
                c->next = next;
                if (next) {
                    struct oracle_Cell *nn = OCELL(next);
                    if (nn) nn->prev = cell;
                }
            }
        }
    }

    if (prev) {
        struct oracle_Cell *p = OCELL(prev);
        if (p) {
            p->next = cell;
            if ((uint32_t)(prev + (o32)p->size) == cell) {
                p->size += c->size;
                p->next = next;
                if (next) {
                    struct oracle_Cell *n = OCELL(next);
                    if (n) n->prev = prev;
                }
            }
        }
        return list;
    }

    return cell;
}

static int oracle_DLOverlap(o32 list, o32 start, o32 end) {
    for (o32 it = list; it != 0; ) {
        struct oracle_Cell *c = OCELL(it);
        if (!c) return 1;
        o32 c_start = it;
        o32 c_end = it + (o32)c->size;
        if ((start <= c_start && c_start < end) || (start < c_end && c_end <= end)) {
            return 1;
        }
        it = c->next;
    }
    return 0;
}

static int32_t oracle_DLSize(o32 list) {
    int32_t total = 0;
    for (o32 it = list; it != 0; ) {
        struct oracle_Cell *c = OCELL(it);
        if (!c) return total;
        total += c->size;
        it = c->next;
    }
    return total;
}

/* ── API (offset-pointer version) ── */

static o32 oracle_OSInitAlloc(o32 arenaStart, o32 arenaEnd, int32_t maxHeaps) {
    if (maxHeaps <= 0) return 0;
    if (arenaStart >= arenaEnd) return 0;

    uint32_t arraySize = (uint32_t)maxHeaps * (uint32_t)sizeof(struct oracle_HeapDesc);
    oracle_HeapArray = arenaStart;
    oracle_NumHeaps = maxHeaps;

    for (int32_t i = 0; i < oracle_NumHeaps; i++) {
        o32 hd_off = oracle_HeapArray + (o32)(i * (int32_t)sizeof(struct oracle_HeapDesc));
        struct oracle_HeapDesc *hd = OHD(hd_off);
        if (!hd) return 0;
        hd->size = -1;
        hd->free = 0;
        hd->allocated = 0;
    }

    oracle___OSCurrHeap = -1;

    o32 new_start = oracle_HeapArray + (o32)arraySize;
    new_start = (o32)oracle_round_up(new_start, ORACLE_ALIGNMENT);
    oracle_ArenaStart = new_start;
    oracle_ArenaEnd = (o32)oracle_round_down(arenaEnd, ORACLE_ALIGNMENT);

    if ((oracle_ArenaEnd - oracle_ArenaStart) < ORACLE_MINOBJSIZE) return 0;
    return new_start;
}

static int32_t oracle_OSCreateHeap(o32 start, o32 end) {
    if (oracle_HeapArray == 0) return -1;
    if (start >= end) return -1;

    start = (o32)oracle_round_up(start, ORACLE_ALIGNMENT);
    end = (o32)oracle_round_down(end, ORACLE_ALIGNMENT);
    if (start >= end) return -1;
    if ((end - start) < ORACLE_MINOBJSIZE) return -1;

    for (int32_t heap = 0; heap < oracle_NumHeaps; heap++) {
        o32 hd_off = oracle_HeapArray + (o32)(heap * (int32_t)sizeof(struct oracle_HeapDesc));
        struct oracle_HeapDesc *hd = OHD(hd_off);
        if (!hd) return -1;
        if (hd->size < 0) {
            hd->size = (int32_t)(end - start);
            struct oracle_Cell *cell = OCELL(start);
            if (!cell) return -1;
            cell->prev = 0;
            cell->next = 0;
            cell->size = hd->size;
            cell->hd = 0;
            hd->free = start;
            hd->allocated = 0;
            return heap;
        }
    }
    return -1;
}

static int32_t oracle_OSSetCurrentHeap(int32_t heap) {
    int32_t prev = oracle___OSCurrHeap;
    oracle___OSCurrHeap = heap;
    return prev;
}

static o32 oracle_OSAllocFromHeap(int32_t heap, uint32_t size) {
    if ((int32_t)size <= 0) return 0;
    if (oracle_HeapArray == 0) return 0;
    if (heap < 0 || heap >= oracle_NumHeaps) return 0;

    o32 hd_off = oracle_HeapArray + (o32)(heap * (int32_t)sizeof(struct oracle_HeapDesc));
    struct oracle_HeapDesc *hd = OHD(hd_off);
    if (!hd || hd->size < 0) return 0;

    uint32_t req = size + ORACLE_HEADERSIZE;
    req = oracle_round_up(req, ORACLE_ALIGNMENT);

    o32 cell_off = hd->free;
    while (cell_off != 0) {
        struct oracle_Cell *cell = OCELL(cell_off);
        if (!cell) return 0;
        if ((int32_t)req <= cell->size) break;
        cell_off = cell->next;
    }
    if (cell_off == 0) return 0;

    struct oracle_Cell *cell = OCELL(cell_off);
    if (!cell) return 0;

    o32 cell_prev = cell->prev;
    o32 cell_next = cell->next;
    uint32_t cell_size = (uint32_t)cell->size;

    uint32_t leftover = cell_size - req;
    if (leftover < ORACLE_MINOBJSIZE) {
        hd->free = oracle_DLExtract(hd->free, cell_off);
    } else {
        cell->size = (int32_t)req;
        o32 new_cell_off = cell_off + (o32)req;
        struct oracle_Cell *new_cell = OCELL(new_cell_off);
        if (!new_cell) return 0;
        new_cell->size = (int32_t)leftover;
        new_cell->prev = cell_prev;
        new_cell->next = cell_next;
        new_cell->hd = 0;
        if (cell_next) {
            struct oracle_Cell *n = OCELL(cell_next);
            if (n) n->prev = new_cell_off;
        }
        if (cell_prev) {
            struct oracle_Cell *p = OCELL(cell_prev);
            if (p) p->next = new_cell_off;
        } else {
            hd->free = new_cell_off;
        }
    }

    hd->allocated = oracle_DLAddFront(hd->allocated, cell_off);
    cell->hd = hd_off;

    return cell_off + ORACLE_HEADERSIZE;
}

static o32 oracle_OSAlloc(uint32_t size) {
    return oracle_OSAllocFromHeap(oracle___OSCurrHeap, size);
}

static void oracle_OSFreeToHeap(int32_t heap, o32 ptr_off) {
    if (ptr_off == 0) return;
    if (oracle_HeapArray == 0) return;
    if (heap < 0 || heap >= oracle_NumHeaps) return;

    o32 hd_off = oracle_HeapArray + (o32)(heap * (int32_t)sizeof(struct oracle_HeapDesc));
    struct oracle_HeapDesc *hd = OHD(hd_off);
    if (!hd || hd->size < 0) return;

    o32 cell_off = ptr_off - ORACLE_HEADERSIZE;
    struct oracle_Cell *cell = OCELL(cell_off);
    if (!cell) return;

    hd->allocated = oracle_DLExtract(hd->allocated, cell_off);
    cell->hd = 0;
    hd->free = oracle_DLInsert(hd->free, cell_off);
}

static void oracle_OSDestroyHeap(int32_t heap) {
    if (oracle_HeapArray == 0) return;
    if (heap < 0 || heap >= oracle_NumHeaps) return;
    o32 hd_off = oracle_HeapArray + (o32)(heap * (int32_t)sizeof(struct oracle_HeapDesc));
    struct oracle_HeapDesc *hd = OHD(hd_off);
    if (!hd) return;
    hd->size = -1;
    hd->free = 0;
    hd->allocated = 0;
}

static void oracle_OSAddToHeap(int32_t heap, o32 start, o32 end) {
    if (oracle_HeapArray == 0) return;
    if (heap < 0 || heap >= oracle_NumHeaps) return;
    o32 hd_off = oracle_HeapArray + (o32)(heap * (int32_t)sizeof(struct oracle_HeapDesc));
    struct oracle_HeapDesc *hd = OHD(hd_off);
    if (!hd || hd->size < 0) return;

    start = (o32)oracle_round_up(start, ORACLE_ALIGNMENT);
    end = (o32)oracle_round_down(end, ORACLE_ALIGNMENT);
    if (start >= end) return;
    if ((end - start) < ORACLE_MINOBJSIZE) return;

    struct oracle_Cell *cell = OCELL(start);
    if (!cell) return;
    cell->prev = 0;
    cell->next = 0;
    cell->size = (int32_t)(end - start);
    cell->hd = 0;

    hd->size += cell->size;
    hd->free = oracle_DLInsert(hd->free, start);
}

static int32_t oracle_OSCheckHeap(int32_t heap) {
    if (oracle_HeapArray == 0) return -1;
    if (heap < 0 || heap >= oracle_NumHeaps) return -1;

    o32 hd_off = oracle_HeapArray + (o32)(heap * (int32_t)sizeof(struct oracle_HeapDesc));
    struct oracle_HeapDesc *hd = OHD(hd_off);
    if (!hd || hd->size < 0) return -1;

    int32_t total = 0;
    int32_t free_bytes = 0;

    /* allocated list */
    if (hd->allocated) {
        struct oracle_Cell *head = OCELL(hd->allocated);
        if (!head || head->prev != 0) return -1;
    }
    for (o32 it = hd->allocated; it != 0; ) {
        struct oracle_Cell *c = OCELL(it);
        if (!c) return -1;
        if (!oracle_InRange(it, oracle_ArenaStart, oracle_ArenaEnd)) return -1;
        if ((it & (ORACLE_ALIGNMENT - 1)) != 0) return -1;
        if ((c->size & (ORACLE_ALIGNMENT - 1)) != 0) return -1;
        if (c->size < (int32_t)ORACLE_MINOBJSIZE) return -1;
        if (c->hd != hd_off) return -1;
        total += c->size;
        it = c->next;
    }

    /* free list */
    if (hd->free) {
        struct oracle_Cell *head = OCELL(hd->free);
        if (!head || head->prev != 0) return -1;
    }
    for (o32 it = hd->free; it != 0; ) {
        struct oracle_Cell *c = OCELL(it);
        if (!c) return -1;
        if (!oracle_InRange(it, oracle_ArenaStart, oracle_ArenaEnd)) return -1;
        if ((it & (ORACLE_ALIGNMENT - 1)) != 0) return -1;
        if ((c->size & (ORACLE_ALIGNMENT - 1)) != 0) return -1;
        if (c->size < (int32_t)ORACLE_MINOBJSIZE) return -1;
        if (c->hd != 0) return -1;
        total += c->size;
        free_bytes += c->size;
        free_bytes -= (int32_t)ORACLE_HEADERSIZE; /* match SDK contract */
        it = c->next;
    }

    if (total != hd->size) return -1;
    return free_bytes;
}
