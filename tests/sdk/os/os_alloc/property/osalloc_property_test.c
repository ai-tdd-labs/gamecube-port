/*
 * osalloc_property_test.c — Property-based test for OSAlloc (oracle vs port).
 *
 * Single binary compiled with -m32.  Contains BOTH:
 *   - Oracle: exact copy of decomp OSAlloc.c (native 32-bit pointers)
 *   - Port:   sdk_port OSAlloc.c (emulated big-endian via gc_mem)
 *
 * Driven by a seed-based PRNG; compares results after every operation.
 *
 * Test levels (bottom-up):
 *   Level 0 — DL leaf functions (DLAddFront, DLExtract, DLInsert, DLLookup)
 *   Level 1 — Individual API functions (OSAllocFromHeap, OSFreeToHeap, etc.)
 *   Level 2 — Full integration (random mix of all operations)
 *
 * Usage:
 *   osalloc_property_test [--seed=N] [--op=NAME] [--num-runs=N] [-v]
 *
 * Default: --num-runs=500 --op=full
 *
 * Oracle source: external/mp4-decomp/src/dolphin/os/OSAlloc.c
 * Why -m32:      struct sizes match GC exactly (Cell=16, HeapDesc=12),
 *                HeapDescs live in the arena, no hacks needed.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Compile-time check: -m32 required */
_Static_assert(sizeof(void *) == 4, "Must compile with -m32");
_Static_assert(sizeof(long) == 4,   "Must compile with -m32");

/* ── Oracle (decomp, native 32-bit pointers, HeapDescs in arena) ── */
#include "osalloc_oracle.h"

/* Compile-time check: struct sizes match GC */
_Static_assert(sizeof(struct oracle_Cell) == 16,     "oracle_Cell must be 16 bytes");
_Static_assert(sizeof(struct oracle_HeapDesc) == 12,  "oracle_HeapDesc must be 12 bytes");

/* ── Port (sdk_port, emulated memory) ── */
#include "harness/gc_host_ram.h"
#include "gc_mem.h"
#include "sdk_state.h"

/* Port API */
extern void *OSInitAlloc(void *arenaStart, void *arenaEnd, int maxHeaps);
extern int   OSCreateHeap(void *start, void *end);
extern int   OSSetCurrentHeap(int heap);
extern void *OSAllocFromHeap(int heap, uint32_t size);
extern void  OSFreeToHeap(int heap, void *ptr);
extern long  OSCheckHeap(int heap);
extern void  OSDestroyHeap(int heap);
extern void  OSAddToHeap(int heap, void *start, void *end);
extern volatile int32_t __OSCurrHeap;

/* Port DL helpers (non-static in sdk_port/os/OSAlloc.c) */
extern uint32_t port_DLAddFront(uint32_t list, uint32_t cell);
extern uint32_t port_DLExtract(uint32_t list, uint32_t cell);
extern uint32_t port_DLInsert(uint32_t list, uint32_t cell);
extern uint32_t port_DLLookup(uint32_t list, uint32_t cell);

/* ── PRNG ── */
static uint32_t xorshift32(uint32_t *state) {
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

/* ── Configuration ── */
#define GC_BASE       0x80000000u
#define PORT_ARENA_LO 0x80002000u
#define MAX_ALLOCS    128
#define MAX_OPS       80
#define MAX_HEAPS     8

/* Oracle arena: 1 MiB aligned static buffer. */
#define ORACLE_BUF_SIZE (1 << 20)
static uint8_t oracle_buf[ORACLE_BUF_SIZE] __attribute__((aligned(64)));

/* ── Allocation tracking ── */
typedef struct {
    void    *oracle_ptr;   /* native pointer from oracle */
    void    *port_ptr;     /* GC-address-as-pointer from port */
    int      heap;
} AllocEntry;

static AllocEntry g_allocs[MAX_ALLOCS];
static int g_num_allocs;

/* Per-heap base addresses for offset comparison */
static uint8_t *g_oracle_heap_base[MAX_HEAPS];
static uint32_t g_port_heap_base[MAX_HEAPS];

/* ── Global state ── */
static GcRam g_ram;
static int g_verbose;

static void fail_msg(uint32_t seed, int op_idx, const char *op_name,
                     const char *detail) {
    fprintf(stderr, "[FAIL] seed=0x%08X op=%d/%s %s\n",
            seed, op_idx, op_name, detail);
}

/* ── Reset both implementations ── */
static void reset_all(void) {
    /* Oracle */
    oracle_reset(oracle_buf, ORACLE_BUF_SIZE);

    /* Port */
    gc_ram_free(&g_ram);
    if (gc_ram_init(&g_ram, GC_BASE, 0x02000000u) != 0) {
        fprintf(stderr, "fatal: gc_ram_init\n");
        exit(2);
    }
    gc_mem_set(g_ram.base, g_ram.size, g_ram.buf);
    gc_sdk_state_reset();
    __OSCurrHeap = -1;

    /* Tracking */
    g_num_allocs = 0;
    memset(g_allocs, 0, sizeof(g_allocs));
    memset(g_oracle_heap_base, 0, sizeof(g_oracle_heap_base));
    memset(g_port_heap_base, 0, sizeof(g_port_heap_base));
}

/* ──────────────────────────────────────────────────────────────────────
 * Level 0: DL leaf function tests
 *
 * These test individual linked-list primitives in isolation.
 * Both oracle and port operate on the same logical structure but in
 * different memory:
 *   - Oracle: native 32-bit pointers into oracle_buf
 *   - Port:   emulated gc_mem at PORT_ARENA_LO
 * ────────────────────────────────────────────────────────────────────── */

/* Helper: write an oracle cell at native pointer */
static void oracle_cell_write(struct oracle_Cell *cell, uint32_t prev_idx,
                               uint32_t next_idx, long size,
                               struct oracle_Cell *base, int n_cells) {
    cell->prev = (prev_idx < (uint32_t)n_cells) ? &base[prev_idx] : NULL;
    cell->next = (next_idx < (uint32_t)n_cells) ? &base[next_idx] : NULL;
    cell->size = size;
    cell->hd = NULL;
}

/* Helper: write a port cell at emulated address */
static void port_cell_write(uint32_t addr, uint32_t prev_addr,
                             uint32_t next_addr, uint32_t size) {
    uint8_t *p = gc_mem_ptr(addr, 16);
    if (!p) return;
    /* prev */
    p[0] = (uint8_t)(prev_addr >> 24); p[1] = (uint8_t)(prev_addr >> 16);
    p[2] = (uint8_t)(prev_addr >> 8);  p[3] = (uint8_t)(prev_addr);
    /* next */
    p[4] = (uint8_t)(next_addr >> 24); p[5] = (uint8_t)(next_addr >> 16);
    p[6] = (uint8_t)(next_addr >> 8);  p[7] = (uint8_t)(next_addr);
    /* size */
    p[8]  = (uint8_t)(size >> 24); p[9]  = (uint8_t)(size >> 16);
    p[10] = (uint8_t)(size >> 8);  p[11] = (uint8_t)(size);
    /* hd = 0 */
    p[12] = 0; p[13] = 0; p[14] = 0; p[15] = 0;
}

/* Helper: read port cell fields */
static uint32_t port_cell_prev(uint32_t addr) {
    uint8_t *p = gc_mem_ptr(addr, 16);
    if (!p) return 0;
    return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|(uint32_t)p[3];
}
static uint32_t port_cell_next(uint32_t addr) {
    uint8_t *p = gc_mem_ptr(addr, 16);
    if (!p) return 0;
    return ((uint32_t)p[4]<<24)|((uint32_t)p[5]<<16)|((uint32_t)p[6]<<8)|(uint32_t)p[7];
}
static uint32_t port_cell_size(uint32_t addr) {
    uint8_t *p = gc_mem_ptr(addr, 16);
    if (!p) return 0;
    return ((uint32_t)p[8]<<24)|((uint32_t)p[9]<<16)|((uint32_t)p[10]<<8)|(uint32_t)p[11];
}

/* Compare two linked lists: oracle (native) vs port (emulated).
 * Both should have the same sequence of (offset, size) pairs. */
static int compare_lists(const char *label, uint32_t seed, int op_idx,
                          struct oracle_Cell *o_list, uint32_t p_list,
                          uint8_t *o_base, uint32_t p_base) {
    struct oracle_Cell *o = o_list;
    uint32_t p = p_list;
    int idx = 0;

    while (o != NULL || p != 0) {
        if ((o == NULL) != (p == 0)) {
            char buf[256];
            snprintf(buf, sizeof(buf),
                     "%s list length mismatch at idx=%d (oracle=%s, port=%s)",
                     label, idx, o ? "more" : "end", p ? "more" : "end");
            fail_msg(seed, op_idx, label, buf);
            return 1;
        }

        uint32_t o_off = (uint32_t)((uint8_t *)o - o_base);
        uint32_t p_off = p - p_base;
        if (o_off != p_off) {
            char buf[256];
            snprintf(buf, sizeof(buf),
                     "%s offset mismatch at idx=%d: oracle=0x%X port=0x%X",
                     label, idx, o_off, p_off);
            fail_msg(seed, op_idx, label, buf);
            return 1;
        }

        long o_size = o->size;
        uint32_t p_size = port_cell_size(p);
        if ((uint32_t)o_size != p_size) {
            char buf[256];
            snprintf(buf, sizeof(buf),
                     "%s size mismatch at idx=%d off=0x%X: oracle=%ld port=%u",
                     label, idx, o_off, o_size, p_size);
            fail_msg(seed, op_idx, label, buf);
            return 1;
        }

        o = o->next;
        p = port_cell_next(p);
        idx++;
    }
    return 0;
}

/* Test DLAddFront: build a list by adding N cells to front. */
static int test_DLAddFront(uint32_t seed) {
    uint32_t rng = seed ? seed : 1;
    reset_all();

    int n_cells = 2 + (int)(xorshift32(&rng) % 10);
    uint32_t cell_spacing = 0x40;

    /* Base addresses for cells */
    uint8_t *o_base = oracle_buf;
    uint32_t p_base = PORT_ARENA_LO;

    struct oracle_Cell *o_list = NULL;
    uint32_t p_list = 0;

    for (int i = 0; i < n_cells; i++) {
        /* Oracle: init cell at offset i*spacing */
        struct oracle_Cell *o_cell = (struct oracle_Cell *)(o_base + i * cell_spacing);
        long cell_size = (long)(cell_spacing + (xorshift32(&rng) % 0x100) * 0x20);
        cell_size = (cell_size + 0x1F) & ~0x1FL;
        if (cell_size < 0x40) cell_size = 0x40;
        o_cell->size = cell_size;
        o_cell->hd = NULL;

        /* Port: init cell at equivalent offset */
        uint32_t p_cell = p_base + (uint32_t)(i * cell_spacing);
        port_cell_write(p_cell, 0, 0, (uint32_t)cell_size);

        /* DLAddFront on both */
        o_list = oracle_DLAddFront(o_list, o_cell);
        p_list = port_DLAddFront(p_list, p_cell);
    }

    return compare_lists("DLAddFront", seed, 0, o_list, p_list, o_base, p_base);
}

/* Test DLExtract: build a list, then extract random cells. */
static int test_DLExtract(uint32_t seed) {
    uint32_t rng = seed ? seed : 1;
    reset_all();

    int n_cells = 3 + (int)(xorshift32(&rng) % 8);
    uint32_t cell_spacing = 0x40;

    uint8_t *o_base = oracle_buf;
    uint32_t p_base = PORT_ARENA_LO;

    /* Build list using DLAddFront (already tested) */
    struct oracle_Cell *o_list = NULL;
    uint32_t p_list = 0;

    for (int i = 0; i < n_cells; i++) {
        struct oracle_Cell *o_cell = (struct oracle_Cell *)(o_base + i * cell_spacing);
        o_cell->size = 0x40;
        o_cell->hd = NULL;

        uint32_t p_cell = p_base + (uint32_t)(i * cell_spacing);
        port_cell_write(p_cell, 0, 0, 0x40);

        o_list = oracle_DLAddFront(o_list, o_cell);
        p_list = port_DLAddFront(p_list, p_cell);
    }

    /* Extract cells in random order */
    int remaining = n_cells;
    int indices[16];
    for (int i = 0; i < n_cells; i++) indices[i] = i;

    int n_extract = 1 + (int)(xorshift32(&rng) % (uint32_t)(n_cells - 1));
    for (int e = 0; e < n_extract && remaining > 0; e++) {
        int pick = (int)(xorshift32(&rng) % (uint32_t)remaining);
        int cell_idx = indices[pick];

        struct oracle_Cell *o_cell = (struct oracle_Cell *)(o_base + cell_idx * cell_spacing);
        uint32_t p_cell = p_base + (uint32_t)(cell_idx * cell_spacing);

        o_list = oracle_DLExtract(o_list, o_cell);
        p_list = port_DLExtract(p_list, p_cell);

        /* Remove from indices */
        indices[pick] = indices[remaining - 1];
        remaining--;

        /* Compare after each extraction */
        if (compare_lists("DLExtract", seed, e, o_list, p_list, o_base, p_base) != 0) {
            return 1;
        }
    }
    return 0;
}

/* Test DLInsert: insert cells in random address order, verify coalescing. */
static int test_DLInsert(uint32_t seed) {
    uint32_t rng = seed ? seed : 1;
    reset_all();

    /* Use larger spacing so cells can have meaningful sizes for coalescing */
    int n_cells = 3 + (int)(xorshift32(&rng) % 6);
    uint32_t cell_spacing = 0x60; /* must be >= MINOBJSIZE + alignment */

    uint8_t *o_base = oracle_buf;
    uint32_t p_base = PORT_ARENA_LO;

    struct oracle_Cell *o_list = NULL;
    uint32_t p_list = 0;

    /* Decide which cells should be adjacent (for coalescing testing).
     * We'll insert cells in random order but with sizes that sometimes
     * make them exactly adjacent. */
    int order[16];
    for (int i = 0; i < n_cells; i++) order[i] = i;
    /* Shuffle insertion order */
    for (int i = n_cells - 1; i > 0; i--) {
        int j = (int)(xorshift32(&rng) % (uint32_t)(i + 1));
        int tmp = order[i]; order[i] = order[j]; order[j] = tmp;
    }

    for (int ins = 0; ins < n_cells; ins++) {
        int idx = order[ins];

        /* Cell address */
        uint32_t off = (uint32_t)idx * cell_spacing;
        struct oracle_Cell *o_cell = (struct oracle_Cell *)(o_base + off);
        uint32_t p_cell = p_base + off;

        /* Size: either exactly cell_spacing (adjacent to next) or smaller */
        long cell_size;
        if (xorshift32(&rng) % 3 == 0) {
            /* Make adjacent to next cell for coalescing */
            cell_size = (long)cell_spacing;
        } else {
            cell_size = 0x40; /* minimum, won't coalesce */
        }
        o_cell->size = cell_size;
        o_cell->hd = NULL;
        port_cell_write(p_cell, 0, 0, (uint32_t)cell_size);

        o_list = oracle_DLInsert(o_list, o_cell);
        p_list = port_DLInsert(p_list, p_cell);

        if (compare_lists("DLInsert", seed, ins, o_list, p_list, o_base, p_base) != 0) {
            return 1;
        }
    }
    return 0;
}

/* Test DLLookup: build a list, then search for cells. */
static int test_DLLookup(uint32_t seed) {
    uint32_t rng = seed ? seed : 1;
    reset_all();

    int n_cells = 3 + (int)(xorshift32(&rng) % 8);
    uint32_t cell_spacing = 0x40;

    uint8_t *o_base = oracle_buf;
    uint32_t p_base = PORT_ARENA_LO;

    struct oracle_Cell *o_list = NULL;
    uint32_t p_list = 0;

    for (int i = 0; i < n_cells; i++) {
        struct oracle_Cell *o_cell = (struct oracle_Cell *)(o_base + i * cell_spacing);
        o_cell->size = 0x40;
        o_cell->hd = NULL;

        uint32_t p_cell = p_base + (uint32_t)(i * cell_spacing);
        port_cell_write(p_cell, 0, 0, 0x40);

        o_list = oracle_DLAddFront(o_list, o_cell);
        p_list = port_DLAddFront(p_list, p_cell);
    }

    /* Lookup each cell + some non-existent ones */
    for (int i = 0; i < n_cells + 3; i++) {
        int idx = (i < n_cells) ? i : n_cells + i; /* out of range for extras */
        struct oracle_Cell *o_cell = (struct oracle_Cell *)(o_base + idx * cell_spacing);
        uint32_t p_cell = p_base + (uint32_t)(idx * cell_spacing);

        struct oracle_Cell *o_found = oracle_DLLookup(o_list, o_cell);
        uint32_t p_found = port_DLLookup(p_list, p_cell);

        int o_ok = (o_found != NULL);
        int p_ok = (p_found != 0);
        if (o_ok != p_ok) {
            char buf[256];
            snprintf(buf, sizeof(buf),
                     "DLLookup idx=%d: oracle=%s port=%s",
                     idx, o_ok ? "found" : "not found", p_ok ? "found" : "not found");
            fail_msg(seed, i, "DLLookup", buf);
            return 1;
        }
    }
    return 0;
}

/* ──────────────────────────────────────────────────────────────────────
 * Level 1+2: API and full integration tests
 * ────────────────────────────────────────────────────────────────────── */

/* Setup: init arenas and create heaps in both oracle and port.
 * Returns number of heaps created, or -1 on mismatch. */
static int setup_heaps(uint32_t *rng, uint32_t seed, int *actual_heaps) {
    uint32_t arena_size = (xorshift32(rng) % (0x20000 - 0x2000)) + 0x2000;
    arena_size &= ~0x1Fu;

    int num_heaps = 1 + (int)(xorshift32(rng) % 3);
    if (num_heaps > MAX_HEAPS) num_heaps = MAX_HEAPS;

    uint8_t *oracle_lo = oracle_buf;
    uint8_t *oracle_hi = oracle_buf + arena_size;
    uint32_t port_lo = PORT_ARENA_LO;
    uint32_t port_hi = PORT_ARENA_LO + arena_size;

    void *oracle_new_lo = oracle_OSInitAlloc(oracle_lo, oracle_hi, num_heaps);
    void *port_new_lo   = OSInitAlloc((void *)(uintptr_t)port_lo,
                                      (void *)(uintptr_t)port_hi, num_heaps);

    uint32_t oracle_off = (uint32_t)((uint8_t *)oracle_new_lo - oracle_lo);
    uint32_t port_off   = (uint32_t)((uintptr_t)port_new_lo - port_lo);

    if (oracle_off != port_off) {
        char buf[256];
        snprintf(buf, sizeof(buf),
                 "OSInitAlloc offset mismatch: oracle=0x%X port=0x%X",
                 oracle_off, port_off);
        fail_msg(seed, 0, "OSInitAlloc", buf);
        return -1;
    }

    uint32_t usable = arena_size - oracle_off;
    uint32_t per_heap = (usable / (uint32_t)num_heaps) & ~0x1Fu;

    if (per_heap < 0x80) {
        *actual_heaps = 0;
        return 0; /* skip */
    }

    *actual_heaps = 0;
    for (int h = 0; h < num_heaps; h++) {
        uint32_t h_off_lo = oracle_off + (uint32_t)h * per_heap;
        uint32_t h_off_hi = (h == num_heaps - 1)
                            ? arena_size
                            : oracle_off + (uint32_t)(h + 1) * per_heap;

        uint8_t *o_lo = oracle_lo + h_off_lo;
        uint8_t *o_hi = oracle_lo + h_off_hi;
        uint32_t p_lo = port_lo + h_off_lo;
        uint32_t p_hi = port_lo + h_off_hi;

        int oh = oracle_OSCreateHeap(o_lo, o_hi);
        int ph = OSCreateHeap((void *)(uintptr_t)p_lo, (void *)(uintptr_t)p_hi);

        if (oh != ph) {
            char buf[256];
            snprintf(buf, sizeof(buf),
                     "OSCreateHeap[%d] mismatch: oracle=%d port=%d", h, oh, ph);
            fail_msg(seed, 0, "OSCreateHeap", buf);
            return -1;
        }
        if (oh >= 0 && oh < MAX_HEAPS) {
            g_oracle_heap_base[oh] = o_lo;
            g_port_heap_base[oh]   = p_lo;
            (*actual_heaps)++;
        }
    }

    if (*actual_heaps > 0) {
        oracle_OSSetCurrentHeap(0);
        OSSetCurrentHeap(0);
    }

    return 0;
}

/* Check heap integrity across all active heaps */
static int check_all_heaps(uint32_t seed, int op_idx, int actual_heaps) {
    for (int h = 0; h < actual_heaps; h++) {
        long oc = oracle_OSCheckHeap(h);
        long pc = OSCheckHeap(h);
        if (oc != pc) {
            char buf[256];
            snprintf(buf, sizeof(buf),
                     "OSCheckHeap(heap=%d): oracle=%ld port=%ld", h, oc, pc);
            fail_msg(seed, op_idx, "OSCheckHeap", buf);
            return 1;
        }
    }
    return 0;
}

/* ── Test: OSAllocFromHeap focused ── */
static int test_OSAllocFromHeap(uint32_t seed) {
    uint32_t rng = seed ? seed : 1;
    reset_all();

    int actual_heaps;
    if (setup_heaps(&rng, seed, &actual_heaps) < 0) return 1;
    if (actual_heaps == 0) return -1;

    if (check_all_heaps(seed, 0, actual_heaps) != 0) return 1;

    /* Do a sequence of allocations */
    int n_ops = 10 + (int)(xorshift32(&rng) % 40);
    for (int i = 0; i < n_ops; i++) {
        int heap = (int)(xorshift32(&rng) % (uint32_t)actual_heaps);
        uint32_t alloc_size = 0x20 + (xorshift32(&rng) % (0x800 - 0x20));
        alloc_size = (alloc_size + 0x1F) & ~0x1Fu;

        void *optr = oracle_OSAllocFromHeap(heap, alloc_size);
        void *pptr = OSAllocFromHeap(heap, alloc_size);

        int ook = (optr != NULL);
        int pok = (pptr != NULL);
        if (ook != pok) {
            char buf[256];
            snprintf(buf, sizeof(buf),
                     "Alloc(heap=%d,sz=0x%X) success: o=%d p=%d",
                     heap, alloc_size, ook, pok);
            fail_msg(seed, i, "OSAllocFromHeap", buf);
            return 1;
        }

        if (ook) {
            uint32_t o_off = (uint32_t)((uint8_t *)optr - g_oracle_heap_base[heap]);
            uint32_t p_off = (uint32_t)((uintptr_t)pptr - g_port_heap_base[heap]);
            if (o_off != p_off) {
                char buf[256];
                snprintf(buf, sizeof(buf),
                         "Alloc(heap=%d,sz=0x%X) offset: o=0x%X p=0x%X",
                         heap, alloc_size, o_off, p_off);
                fail_msg(seed, i, "OSAllocFromHeap", buf);
                return 1;
            }

            if (g_num_allocs < MAX_ALLOCS) {
                g_allocs[g_num_allocs].oracle_ptr = optr;
                g_allocs[g_num_allocs].port_ptr   = pptr;
                g_allocs[g_num_allocs].heap       = heap;
                g_num_allocs++;
            }
        }

        if (check_all_heaps(seed, i, actual_heaps) != 0) return 1;
    }
    return 0;
}

/* ── Test: OSFreeToHeap focused ── */
static int test_OSFreeToHeap(uint32_t seed) {
    uint32_t rng = seed ? seed : 1;
    reset_all();

    int actual_heaps;
    if (setup_heaps(&rng, seed, &actual_heaps) < 0) return 1;
    if (actual_heaps == 0) return -1;

    /* First allocate some blocks */
    int n_alloc = 5 + (int)(xorshift32(&rng) % 20);
    for (int i = 0; i < n_alloc && g_num_allocs < MAX_ALLOCS; i++) {
        int heap = (int)(xorshift32(&rng) % (uint32_t)actual_heaps);
        uint32_t alloc_size = 0x20 + (xorshift32(&rng) % (0x400 - 0x20));
        alloc_size = (alloc_size + 0x1F) & ~0x1Fu;

        void *optr = oracle_OSAllocFromHeap(heap, alloc_size);
        void *pptr = OSAllocFromHeap(heap, alloc_size);

        if (optr && pptr) {
            g_allocs[g_num_allocs].oracle_ptr = optr;
            g_allocs[g_num_allocs].port_ptr   = pptr;
            g_allocs[g_num_allocs].heap       = heap;
            g_num_allocs++;
        }
    }

    if (check_all_heaps(seed, 0, actual_heaps) != 0) return 1;

    /* Now free them in random order */
    int n_free = g_num_allocs;
    for (int i = 0; i < n_free && g_num_allocs > 0; i++) {
        int idx = (int)(xorshift32(&rng) % (uint32_t)g_num_allocs);
        AllocEntry *e = &g_allocs[idx];

        oracle_OSFreeToHeap(e->heap, e->oracle_ptr);
        OSFreeToHeap(e->heap, e->port_ptr);

        g_allocs[idx] = g_allocs[g_num_allocs - 1];
        g_num_allocs--;

        if (check_all_heaps(seed, i, actual_heaps) != 0) return 1;
    }
    return 0;
}

/* ── Test: OSCheckHeap focused ── */
static int test_OSCheckHeap(uint32_t seed) {
    /* OSCheckHeap is already verified in every other test via check_all_heaps.
     * This test does a focused sequence: alloc, check, free, check. */
    uint32_t rng = seed ? seed : 1;
    reset_all();

    int actual_heaps;
    if (setup_heaps(&rng, seed, &actual_heaps) < 0) return 1;
    if (actual_heaps == 0) return -1;

    for (int i = 0; i < 20; i++) {
        int heap = (int)(xorshift32(&rng) % (uint32_t)actual_heaps);
        uint32_t alloc_size = 0x20 + (xorshift32(&rng) % (0x200 - 0x20));
        alloc_size = (alloc_size + 0x1F) & ~0x1Fu;

        void *optr = oracle_OSAllocFromHeap(heap, alloc_size);
        void *pptr = OSAllocFromHeap(heap, alloc_size);

        if (check_all_heaps(seed, i, actual_heaps) != 0) return 1;

        if (optr && pptr) {
            oracle_OSFreeToHeap(heap, optr);
            OSFreeToHeap(heap, pptr);

            if (check_all_heaps(seed, i, actual_heaps) != 0) return 1;
        }
    }
    return 0;
}

/* ── Test: OSDestroyHeap focused ── */
static int test_OSDestroyHeap(uint32_t seed) {
    uint32_t rng = seed ? seed : 1;
    reset_all();

    int actual_heaps;
    if (setup_heaps(&rng, seed, &actual_heaps) < 0) return 1;
    if (actual_heaps < 2) return -1; /* need at least 2 heaps */

    /* Allocate on each heap */
    for (int h = 0; h < actual_heaps && g_num_allocs < MAX_ALLOCS; h++) {
        uint32_t alloc_size = 0x40;
        void *optr = oracle_OSAllocFromHeap(h, alloc_size);
        void *pptr = OSAllocFromHeap(h, alloc_size);
        if (optr && pptr) {
            g_allocs[g_num_allocs].oracle_ptr = optr;
            g_allocs[g_num_allocs].port_ptr   = pptr;
            g_allocs[g_num_allocs].heap       = h;
            g_num_allocs++;
        }
    }

    /* Destroy one heap */
    int destroy_h = (int)(xorshift32(&rng) % (uint32_t)actual_heaps);
    oracle_OSDestroyHeap(destroy_h);
    OSDestroyHeap(destroy_h);

    /* Check: destroyed heap should return -1, others should be fine */
    long oc = oracle_OSCheckHeap(destroy_h);
    long pc = OSCheckHeap(destroy_h);
    if (oc != pc) {
        char buf[256];
        snprintf(buf, sizeof(buf),
                 "Destroyed heap %d check: oracle=%ld port=%ld", destroy_h, oc, pc);
        fail_msg(seed, 0, "OSDestroyHeap", buf);
        return 1;
    }

    /* Other heaps should still be valid */
    for (int h = 0; h < actual_heaps; h++) {
        if (h == destroy_h) continue;
        oc = oracle_OSCheckHeap(h);
        pc = OSCheckHeap(h);
        if (oc != pc) {
            char buf[256];
            snprintf(buf, sizeof(buf),
                     "After destroy(%d), heap %d check: oracle=%ld port=%ld",
                     destroy_h, h, oc, pc);
            fail_msg(seed, 0, "OSDestroyHeap", buf);
            return 1;
        }
    }
    return 0;
}

/* ── Test: OSAddToHeap focused ── */
static int test_OSAddToHeap(uint32_t seed) {
    uint32_t rng = seed ? seed : 1;
    reset_all();

    /* We need a specific setup: init with 1 heap taking part of the arena,
     * then add more memory to it. */
    uint32_t arena_size = 0x10000;
    arena_size &= ~0x1Fu;

    int num_heaps = 2;
    uint8_t *oracle_lo = oracle_buf;
    uint8_t *oracle_hi = oracle_buf + arena_size;
    uint32_t port_lo = PORT_ARENA_LO;
    uint32_t port_hi = PORT_ARENA_LO + arena_size;

    void *oracle_new_lo = oracle_OSInitAlloc(oracle_lo, oracle_hi, num_heaps);
    void *port_new_lo   = OSInitAlloc((void *)(uintptr_t)port_lo,
                                      (void *)(uintptr_t)port_hi, num_heaps);

    uint32_t oracle_off = (uint32_t)((uint8_t *)oracle_new_lo - oracle_lo);
    uint32_t port_off   = (uint32_t)((uintptr_t)port_new_lo - port_lo);
    if (oracle_off != port_off) {
        fail_msg(seed, 0, "OSAddToHeap", "OSInitAlloc offset mismatch");
        return 1;
    }

    /* Create heap in first half */
    uint32_t mid = oracle_off + ((arena_size - oracle_off) / 2);
    mid &= ~0x1Fu;

    uint8_t *o_h0_lo = oracle_lo + oracle_off;
    uint8_t *o_h0_hi = oracle_lo + mid;
    uint32_t p_h0_lo = port_lo + oracle_off;
    uint32_t p_h0_hi = port_lo + mid;

    int oh = oracle_OSCreateHeap(o_h0_lo, o_h0_hi);
    int ph = OSCreateHeap((void *)(uintptr_t)p_h0_lo, (void *)(uintptr_t)p_h0_hi);
    if (oh != ph || oh < 0) {
        fail_msg(seed, 0, "OSAddToHeap", "OSCreateHeap mismatch");
        return 1;
    }

    g_oracle_heap_base[oh] = o_h0_lo;
    g_port_heap_base[oh]   = p_h0_lo;

    /* Do some allocations */
    for (int i = 0; i < 3 && g_num_allocs < MAX_ALLOCS; i++) {
        uint32_t sz = 0x40 + (xorshift32(&rng) % 0x100);
        sz = (sz + 0x1F) & ~0x1Fu;
        void *optr = oracle_OSAllocFromHeap(oh, sz);
        void *pptr = OSAllocFromHeap(oh, sz);
        if (optr && pptr) {
            g_allocs[g_num_allocs].oracle_ptr = optr;
            g_allocs[g_num_allocs].port_ptr   = pptr;
            g_allocs[g_num_allocs].heap       = oh;
            g_num_allocs++;
        }
    }

    /* Now add the second half of memory to this heap */
    uint8_t *o_add_lo = oracle_lo + mid;
    uint8_t *o_add_hi = oracle_lo + arena_size;
    uint32_t p_add_lo = port_lo + mid;
    uint32_t p_add_hi = port_lo + arena_size;

    oracle_OSAddToHeap(oh, o_add_lo, o_add_hi);
    OSAddToHeap(oh, (void *)(uintptr_t)p_add_lo, (void *)(uintptr_t)p_add_hi);

    /* Check heap integrity */
    long oc = oracle_OSCheckHeap(oh);
    long pc = OSCheckHeap(oh);
    if (oc != pc) {
        char buf[256];
        snprintf(buf, sizeof(buf),
                 "After AddToHeap: oracle=%ld port=%ld", oc, pc);
        fail_msg(seed, 0, "OSAddToHeap", buf);
        return 1;
    }

    /* Allocate more (should now fit in the expanded heap) */
    for (int i = 0; i < 5 && g_num_allocs < MAX_ALLOCS; i++) {
        uint32_t sz = 0x40 + (xorshift32(&rng) % 0x200);
        sz = (sz + 0x1F) & ~0x1Fu;
        void *optr = oracle_OSAllocFromHeap(oh, sz);
        void *pptr = OSAllocFromHeap(oh, sz);

        int ook = (optr != NULL);
        int pok = (pptr != NULL);
        if (ook != pok) {
            char buf[256];
            snprintf(buf, sizeof(buf),
                     "Post-add alloc sz=0x%X: o=%d p=%d", sz, ook, pok);
            fail_msg(seed, i, "OSAddToHeap", buf);
            return 1;
        }
        if (optr && pptr) {
            uint32_t o_off2 = (uint32_t)((uint8_t *)optr - g_oracle_heap_base[oh]);
            uint32_t p_off2 = (uint32_t)((uintptr_t)pptr - g_port_heap_base[oh]);
            if (o_off2 != p_off2) {
                char buf[256];
                snprintf(buf, sizeof(buf),
                         "Post-add alloc offset: o=0x%X p=0x%X", o_off2, p_off2);
                fail_msg(seed, i, "OSAddToHeap", buf);
                return 1;
            }
            g_allocs[g_num_allocs].oracle_ptr = optr;
            g_allocs[g_num_allocs].port_ptr   = pptr;
            g_allocs[g_num_allocs].heap       = oh;
            g_num_allocs++;
        }
    }

    oc = oracle_OSCheckHeap(oh);
    pc = OSCheckHeap(oh);
    if (oc != pc) {
        char buf[256];
        snprintf(buf, sizeof(buf),
                 "Final check: oracle=%ld port=%ld", oc, pc);
        fail_msg(seed, 0, "OSAddToHeap", buf);
        return 1;
    }
    return 0;
}

/* ── Full integration test (Level 2) ── */
static int test_full(uint32_t seed) {
    uint32_t rng = seed ? seed : 1;
    reset_all();

    int actual_heaps;
    if (setup_heaps(&rng, seed, &actual_heaps) < 0) return 1;
    if (actual_heaps == 0) return -1;

    if (check_all_heaps(seed, 0, actual_heaps) != 0) return 1;

    int num_ops = 10 + (int)(xorshift32(&rng) % (MAX_OPS - 10));

    for (int i = 0; i < num_ops; i++) {
        uint32_t action_rng = xorshift32(&rng) % 100;
        int heap = (int)(xorshift32(&rng) % (uint32_t)actual_heaps);

        int do_alloc = 0, do_free = 0;
        if (g_num_allocs == 0 || action_rng < 55) {
            do_alloc = 1;
        } else if (g_num_allocs >= MAX_ALLOCS || action_rng < 85) {
            do_free = 1;
        }

        if (do_alloc) {
            uint32_t alloc_size = 0x20 + (xorshift32(&rng) % (0x1000 - 0x20));
            alloc_size = (alloc_size + 0x1F) & ~0x1Fu;

            void *optr = oracle_OSAllocFromHeap(heap, alloc_size);
            void *pptr = OSAllocFromHeap(heap, alloc_size);

            int ook = (optr != NULL);
            int pok = (pptr != NULL);

            if (ook != pok) {
                char buf[256];
                snprintf(buf, sizeof(buf),
                         "Alloc(heap=%d,sz=0x%X) success: o=%d p=%d",
                         heap, alloc_size, ook, pok);
                fail_msg(seed, i, "OSAllocFromHeap", buf);
                return 1;
            }

            if (ook) {
                uint32_t o_off = (uint32_t)((uint8_t *)optr - g_oracle_heap_base[heap]);
                uint32_t p_off = (uint32_t)((uintptr_t)pptr - g_port_heap_base[heap]);

                if (o_off != p_off) {
                    char buf[256];
                    snprintf(buf, sizeof(buf),
                             "Alloc(heap=%d,sz=0x%X) offset: o=0x%X p=0x%X",
                             heap, alloc_size, o_off, p_off);
                    fail_msg(seed, i, "OSAllocFromHeap", buf);
                    return 1;
                }

                if (g_num_allocs < MAX_ALLOCS) {
                    g_allocs[g_num_allocs].oracle_ptr = optr;
                    g_allocs[g_num_allocs].port_ptr   = pptr;
                    g_allocs[g_num_allocs].heap       = heap;
                    g_num_allocs++;
                }
            }
        } else if (do_free && g_num_allocs > 0) {
            int idx = (int)(xorshift32(&rng) % (uint32_t)g_num_allocs);
            AllocEntry *e = &g_allocs[idx];

            oracle_OSFreeToHeap(e->heap, e->oracle_ptr);
            OSFreeToHeap(e->heap, e->port_ptr);

            g_allocs[idx] = g_allocs[g_num_allocs - 1];
            g_num_allocs--;
        }

        if (check_all_heaps(seed, i, actual_heaps) != 0) return 1;
    }
    return 0;
}

/* ── Dispatch table ── */
typedef struct {
    const char *name;
    int (*func)(uint32_t seed);
} TestEntry;

static const TestEntry tests[] = {
    /* Level 0: DL leaf functions */
    { "DLAddFront",        test_DLAddFront },
    { "DLExtract",         test_DLExtract },
    { "DLInsert",          test_DLInsert },
    { "DLLookup",          test_DLLookup },
    /* Level 1: Individual API functions */
    { "OSAllocFromHeap",   test_OSAllocFromHeap },
    { "OSFreeToHeap",      test_OSFreeToHeap },
    { "OSCheckHeap",       test_OSCheckHeap },
    { "OSDestroyHeap",     test_OSDestroyHeap },
    { "OSAddToHeap",       test_OSAddToHeap },
    /* Level 2: Full integration */
    { "full",              test_full },
};
#define NUM_TESTS (sizeof(tests) / sizeof(tests[0]))

static const TestEntry *find_test(const char *name) {
    for (int i = 0; i < (int)NUM_TESTS; i++) {
        if (strcmp(tests[i].name, name) == 0) return &tests[i];
    }
    return NULL;
}

/* ── CLI ── */
static uint32_t parse_u32_arg(const char *s) {
    return (uint32_t)strtoul(s, NULL, 0);
}

int main(int argc, char **argv) {
    uint32_t seed = 0;
    int have_seed = 0;
    const char *op_filter = "full";
    int num_runs = 500;
    g_verbose = 0;

    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--seed=", 7) == 0) {
            seed = parse_u32_arg(argv[i] + 7);
            have_seed = 1;
        } else if (strncmp(argv[i], "--op=", 5) == 0) {
            op_filter = argv[i] + 5;
        } else if (strncmp(argv[i], "--num-runs=", 11) == 0) {
            num_runs = (int)parse_u32_arg(argv[i] + 11);
        } else if (strcmp(argv[i], "--verbose") == 0 || strcmp(argv[i], "-v") == 0) {
            g_verbose = 1;
        } else {
            fprintf(stderr, "Usage: %s [--seed=N] [--op=NAME] [--num-runs=N] [-v]\n",
                    argv[0]);
            fprintf(stderr, "  ops: ");
            for (int j = 0; j < (int)NUM_TESTS; j++) {
                fprintf(stderr, "%s%s", tests[j].name,
                        j < (int)NUM_TESTS - 1 ? ", " : "\n");
            }
            return 1;
        }
    }

    const TestEntry *test = find_test(op_filter);
    if (!test) {
        fprintf(stderr, "Unknown op: %s\n", op_filter);
        fprintf(stderr, "Available: ");
        for (int j = 0; j < (int)NUM_TESTS; j++) {
            fprintf(stderr, "%s%s", tests[j].name,
                    j < (int)NUM_TESTS - 1 ? ", " : "\n");
        }
        return 1;
    }

    if (have_seed) {
        g_verbose = 1;
        printf("[property] OSAlloc %s: seed=0x%08X\n", test->name, seed);
        int rc = test->func(seed);
        gc_ram_free(&g_ram);
        if (rc == 0) {
            printf("[PASS] seed=0x%08X\n", seed);
        } else if (rc > 0) {
            printf("[FAIL] seed=0x%08X\n", seed);
        } else {
            printf("[SKIP] seed=0x%08X (arena too small)\n", seed);
        }
        return (rc > 0) ? 1 : 0;
    }

    printf("[property] OSAlloc %s: %d seeds\n", test->name, num_runs);
    int passed = 0, failed = 0, skipped = 0;

    for (int i = 1; i <= num_runs; i++) {
        int rc = test->func((uint32_t)i);
        if (rc == 0) {
            passed++;
            if (g_verbose) printf("[PASS] seed=0x%08X\n", (uint32_t)i);
        } else if (rc > 0) {
            failed++;
            printf("[FAIL] seed=0x%08X\n", (uint32_t)i);
        } else {
            skipped++;
        }
    }

    printf("=== %d/%d PASSED", passed, num_runs);
    if (failed > 0)  printf(", %d FAILED", failed);
    if (skipped > 0) printf(", %d SKIPPED", skipped);
    printf(" ===\n");

    gc_ram_free(&g_ram);
    return (failed > 0) ? 1 : 0;
}
