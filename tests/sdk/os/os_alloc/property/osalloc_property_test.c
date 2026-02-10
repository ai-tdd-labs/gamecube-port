/*
 * osalloc_property_test.c — Property-style parity test for OSAlloc (oracle vs port).
 *
 * Portable (no -m32):
 * - Oracle uses 32-bit offset pointers in an arena buffer.
 * - Port uses sdk_port OSAlloc + emulated big-endian RAM (gc_mem).
 *
 * This is a deterministic "1-button loop" test suite:
 * - leaf-level DL operations (L0) to lock list/coalescing semantics
 * - API-level operations (L1) for allocator behaviors beyond a single seed
 * - integration mix (L2) for regression catching
 *
 * This is semantic parity (offsets / free counts), not a Dolphin RAM-dump oracle.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "osalloc_oracle_offsets.h"

#include "harness/gc_host_ram.h"
#include "gc_mem.h"
#include "sdk_state.h"

/* Port API (sdk_port) */
extern void *OSInitAlloc(void *arenaStart, void *arenaEnd, int maxHeaps);
extern int   OSCreateHeap(void *start, void *end);
extern int   OSSetCurrentHeap(int heap);
extern void *OSAllocFromHeap(int heap, uint32_t size);
extern void  OSFreeToHeap(int heap, void *ptr);
extern long  OSCheckHeap(int heap);
extern void  OSDestroyHeap(int heap);
extern void  OSAddToHeap(int heap, void *start, void *end);
extern volatile int32_t __OSCurrHeap;

/* Port DL helpers (exported for leaf tests) */
extern uint32_t port_DLAddFront(uint32_t list, uint32_t cell);
extern uint32_t port_DLExtract(uint32_t list, uint32_t cell);
extern uint32_t port_DLInsert(uint32_t list, uint32_t cell);
extern uint32_t port_DLLookup(uint32_t list, uint32_t cell);

/* ── Config ── */

#define GC_BASE       0x80000000u
#define PORT_ARENA_LO 0x80002000u
#define PORT_ARENA_HI (PORT_ARENA_LO + 0x00100000u)

#define ORACLE_BUF_SIZE (1u << 20) /* 1 MiB */
#define MAX_ALLOCS      256

static uint8_t oracle_buf[ORACLE_BUF_SIZE] __attribute__((aligned(64)));
static GcRam g_ram;

/* ── PRNG (deterministic, cheap) ── */

static uint32_t xorshift32(uint32_t *state) {
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

static void die(const char *msg) {
    fprintf(stderr, "fatal: %s\n", msg);
    exit(2);
}

static void reset_all(void) {
    /* Oracle */
    oracle_reset(oracle_buf, ORACLE_BUF_SIZE);

    /* Port */
    gc_ram_free(&g_ram);
    if (gc_ram_init(&g_ram, GC_BASE, 0x02000000u) != 0) {
        die("gc_ram_init failed");
    }
    gc_mem_set(g_ram.base, g_ram.size, g_ram.buf);
    gc_sdk_state_reset();
    __OSCurrHeap = -1;
}

static void fail_case(uint32_t seed, int run_idx, int step_idx, const char *op, const char *msg) {
    fprintf(stderr, "[FAIL] seed=0x%08X run=%d step=%d op=%s %s\n", seed, run_idx, step_idx, op, msg);
}

/* ── Big-endian helpers for port leaf tests ── */

static inline uint32_t load_u32be(uint32_t addr) {
    uint8_t *p = gc_mem_ptr(addr, 4);
    if (!p) return 0;
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static inline void store_u32be(uint32_t addr, uint32_t v) {
    uint8_t *p = gc_mem_ptr(addr, 4);
    if (!p) return;
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)(v >> 0);
}

/* Port Cell layout (16 bytes): prev,next,size,hd (all u32 BE) */
static void port_cell_write(uint32_t addr, uint32_t prev, uint32_t next, uint32_t size, uint32_t hd) {
    store_u32be(addr + 0, prev);
    store_u32be(addr + 4, next);
    store_u32be(addr + 8, size);
    store_u32be(addr + 12, hd);
}
static uint32_t port_cell_prev(uint32_t addr) { return load_u32be(addr + 0); }
static uint32_t port_cell_next(uint32_t addr) { return load_u32be(addr + 4); }
static uint32_t port_cell_size(uint32_t addr) { return load_u32be(addr + 8); }

/* ── Common compare: oracle list (offsets) vs port list (GC addresses) ── */

static int compare_lists(const char *op, uint32_t seed, int run_idx, int step_idx,
                         o32 o_list, uint32_t p_list,
                         o32 o_base, uint32_t p_base) {
    o32 o_prev = 0;
    uint32_t p_prev = 0;
    o32 o = o_list;
    uint32_t p = p_list;
    int idx = 0;

    while (o != 0 || p != 0) {
        if ((o == 0) != (p == 0)) {
            fail_case(seed, run_idx, step_idx, op, "list length mismatch");
            return 1;
        }

        struct oracle_Cell *oc = OCELL(o);
        if (!oc) {
            fail_case(seed, run_idx, step_idx, op, "oracle cell OOB");
            return 1;
        }

        uint32_t o_rel = (uint32_t)(o - o_base);
        uint32_t p_rel = (uint32_t)(p - p_base);
        if (o_rel != p_rel) {
            fail_case(seed, run_idx, step_idx, op, "node offset mismatch");
            return 1;
        }

        if ((uint32_t)oc->size != port_cell_size(p)) {
            fail_case(seed, run_idx, step_idx, op, "node size mismatch");
            return 1;
        }

        /* prev linkage must match */
        if (oc->prev != o_prev) {
            fail_case(seed, run_idx, step_idx, op, "oracle prev link mismatch");
            return 1;
        }
        if (port_cell_prev(p) != p_prev) {
            fail_case(seed, run_idx, step_idx, op, "port prev link mismatch");
            return 1;
        }

        o_prev = o;
        p_prev = p;
        o = oc->next;
        p = port_cell_next(p);
        idx++;
        if (idx > 4096) {
            fail_case(seed, run_idx, step_idx, op, "loop detected");
            return 1;
        }
    }
    return 0;
}

/* ──────────────────────────────────────────────────────────────────────
 * Level 0 — DL leaf operations (portable offsets oracle)
 * ────────────────────────────────────────────────────────────────────── */

static int test_DLAddFront(uint32_t seed) {
    uint32_t rng = seed ? seed : 1;
    reset_all();

    const int n_cells = 2 + (int)(xorshift32(&rng) % 10);
    const uint32_t spacing = 0x40;

    const o32 o_base = 0x2000;
    const uint32_t p_base = PORT_ARENA_LO + 0x2000;

    o32 o_list = 0;
    uint32_t p_list = 0;

    for (int i = 0; i < n_cells; i++) {
        o32 o_cell = o_base + (o32)(i * spacing);
        struct oracle_Cell *oc = OCELL(o_cell);
        if (!oc) return 1;
        uint32_t sz = 0x40u + ((xorshift32(&rng) % 8u) * 0x20u);
        sz = (sz + 31u) & ~31u;
        oc->prev = 0;
        oc->next = 0;
        oc->size = (int32_t)sz;
        oc->hd = 0;

        uint32_t p_cell = p_base + (uint32_t)(i * spacing);
        port_cell_write(p_cell, 0, 0, sz, 0);

        o_list = oracle_DLAddFront(o_list, o_cell);
        p_list = port_DLAddFront(p_list, p_cell);
    }

    return compare_lists("DLAddFront", seed, 0, 0, o_list, p_list, o_base, p_base);
}

static int test_DLExtract(uint32_t seed) {
    uint32_t rng = seed ? seed : 1;
    reset_all();

    const int n_cells = 3 + (int)(xorshift32(&rng) % 8);
    const uint32_t spacing = 0x40;
    const o32 o_base = 0x2000;
    const uint32_t p_base = PORT_ARENA_LO + 0x2000;

    o32 o_list = 0;
    uint32_t p_list = 0;

    for (int i = 0; i < n_cells; i++) {
        o32 o_cell = o_base + (o32)(i * spacing);
        struct oracle_Cell *oc = OCELL(o_cell);
        if (!oc) return 1;
        oc->prev = 0;
        oc->next = 0;
        oc->size = 0x40;
        oc->hd = 0;

        uint32_t p_cell = p_base + (uint32_t)(i * spacing);
        port_cell_write(p_cell, 0, 0, 0x40, 0);

        o_list = oracle_DLAddFront(o_list, o_cell);
        p_list = port_DLAddFront(p_list, p_cell);
    }

    int remaining = n_cells;
    int indices[16];
    for (int i = 0; i < n_cells; i++) indices[i] = i;

    const int n_extract = 1 + (int)(xorshift32(&rng) % (uint32_t)(n_cells - 1));
    for (int e = 0; e < n_extract && remaining > 0; e++) {
        int pick = (int)(xorshift32(&rng) % (uint32_t)remaining);
        int cell_idx = indices[pick];

        o32 o_cell = o_base + (o32)(cell_idx * spacing);
        uint32_t p_cell = p_base + (uint32_t)(cell_idx * spacing);

        o_list = oracle_DLExtract(o_list, o_cell);
        p_list = port_DLExtract(p_list, p_cell);

        indices[pick] = indices[remaining - 1];
        remaining--;

        if (compare_lists("DLExtract", seed, 0, e, o_list, p_list, o_base, p_base) != 0) return 1;
    }
    return 0;
}

static int test_DLInsert(uint32_t seed) {
    uint32_t rng = seed ? seed : 1;
    reset_all();

    const int n_cells = 3 + (int)(xorshift32(&rng) % 6);
    const uint32_t spacing = 0x60; /* >= MINOBJSIZE and allows adjacency */
    const o32 o_base = 0x2000;
    const uint32_t p_base = PORT_ARENA_LO + 0x2000;

    o32 o_list = 0;
    uint32_t p_list = 0;

    int order[16];
    for (int i = 0; i < n_cells; i++) order[i] = i;
    for (int i = n_cells - 1; i > 0; i--) {
        int j = (int)(xorshift32(&rng) % (uint32_t)(i + 1));
        int tmp = order[i]; order[i] = order[j]; order[j] = tmp;
    }

    for (int ins = 0; ins < n_cells; ins++) {
        int idx = order[ins];
        o32 o_cell = o_base + (o32)(idx * spacing);
        uint32_t p_cell = p_base + (uint32_t)(idx * spacing);

        uint32_t sz;
        if ((xorshift32(&rng) % 3u) == 0u) {
            sz = spacing; /* exact adjacency, should coalesce */
        } else {
            sz = 0x40u;
        }

        struct oracle_Cell *oc = OCELL(o_cell);
        if (!oc) return 1;
        oc->prev = 0;
        oc->next = 0;
        oc->size = (int32_t)sz;
        oc->hd = 0;
        port_cell_write(p_cell, 0, 0, sz, 0);

        o_list = oracle_DLInsert(o_list, o_cell);
        p_list = port_DLInsert(p_list, p_cell);

        if (compare_lists("DLInsert", seed, 0, ins, o_list, p_list, o_base, p_base) != 0) return 1;
    }

    return 0;
}

static int test_DLLookup(uint32_t seed) {
    uint32_t rng = seed ? seed : 1;
    reset_all();

    const int n_cells = 2 + (int)(xorshift32(&rng) % 10);
    const uint32_t spacing = 0x40;
    const o32 o_base = 0x2000;
    const uint32_t p_base = PORT_ARENA_LO + 0x2000;

    o32 o_list = 0;
    uint32_t p_list = 0;

    for (int i = 0; i < n_cells; i++) {
        o32 o_cell = o_base + (o32)(i * spacing);
        struct oracle_Cell *oc = OCELL(o_cell);
        if (!oc) return 1;
        oc->prev = 0;
        oc->next = 0;
        oc->size = 0x40;
        oc->hd = 0;

        uint32_t p_cell = p_base + (uint32_t)(i * spacing);
        port_cell_write(p_cell, 0, 0, 0x40, 0);

        o_list = oracle_DLAddFront(o_list, o_cell);
        p_list = port_DLAddFront(p_list, p_cell);
    }

    /* Query some present and one absent */
    for (int q = 0; q < 6; q++) {
        int pick = (int)(xorshift32(&rng) % (uint32_t)n_cells);
        o32 o_cell = o_base + (o32)(pick * spacing);
        uint32_t p_cell = p_base + (uint32_t)(pick * spacing);
        if ((oracle_DLLookup(o_list, o_cell) != 0) != (port_DLLookup(p_list, p_cell) != 0)) {
            fail_case(seed, 0, q, "DLLookup", "present mismatch");
            return 1;
        }
    }
    if (oracle_DLLookup(o_list, o_base + (o32)(n_cells * spacing)) != 0) return 1;
    if (port_DLLookup(p_list, p_base + (uint32_t)(n_cells * spacing)) != 0) return 1;
    return 0;
}

/* ──────────────────────────────────────────────────────────────────────
 * Level 1/2 — API/integration sequences
 * ────────────────────────────────────────────────────────────────────── */

typedef struct {
    int heap;
    o32 oracle_ptr; /* oracle offset pointer (header already applied) */
    void *port_ptr; /* GC-address-as-pointer */
} Alloc;

static Alloc g_allocs[MAX_ALLOCS];
static int g_alloc_count;

static void alloc_reset(void) {
    g_alloc_count = 0;
    memset(g_allocs, 0, sizeof(g_allocs));
}

static void alloc_push(int heap, o32 o_ptr, void *p_ptr) {
    if (g_alloc_count >= MAX_ALLOCS) return;
    g_allocs[g_alloc_count].heap = heap;
    g_allocs[g_alloc_count].oracle_ptr = o_ptr;
    g_allocs[g_alloc_count].port_ptr = p_ptr;
    g_alloc_count++;
}

static int alloc_pick_index(uint32_t *rng) {
    if (g_alloc_count <= 0) return -1;
    return (int)(xorshift32(rng) % (uint32_t)g_alloc_count);
}

static void alloc_remove_index(int idx) {
    if (idx < 0 || idx >= g_alloc_count) return;
    g_allocs[idx] = g_allocs[g_alloc_count - 1];
    g_alloc_count--;
}

static int compare_alloc_result(o32 oracle_ptr, void *port_ptr, o32 oracle_heap_base, uint32_t port_heap_base) {
    if ((oracle_ptr == 0) != (port_ptr == NULL)) return 1;
    if (oracle_ptr == 0) return 0;
    uint32_t o_rel = (uint32_t)(oracle_ptr - oracle_heap_base);
    uint32_t p_rel = (uint32_t)((uint32_t)(uintptr_t)port_ptr - port_heap_base);
    return o_rel != p_rel;
}

static int setup_one_heap(o32 o_heap_base, o32 o_heap_end, uint32_t p_heap_base, uint32_t p_heap_end,
                          int32_t *out_o_heap, int *out_p_heap) {
    reset_all();
    alloc_reset();

    if (oracle_OSInitAlloc(0x1000, (o32)(ORACLE_BUF_SIZE - 0x1000), 8) == 0) return 1;
    int32_t o_heap = oracle_OSCreateHeap(o_heap_base, o_heap_end);
    oracle_OSSetCurrentHeap(o_heap);

    OSInitAlloc((void *)(uintptr_t)PORT_ARENA_LO, (void *)(uintptr_t)PORT_ARENA_HI, 8);
    int p_heap = OSCreateHeap((void *)(uintptr_t)p_heap_base, (void *)(uintptr_t)p_heap_end);
    OSSetCurrentHeap(p_heap);

    *out_o_heap = o_heap;
    *out_p_heap = p_heap;
    return (o_heap < 0 || p_heap < 0) ? 1 : 0;
}

static int run_alloc_free_mix(uint32_t seed, int run_idx, int steps, int alloc_only) {
    uint32_t rng = seed ? seed : 1;

    const o32 o_heap_base = 0x4000;
    const o32 o_heap_end  = (o32)(0x4000 + 0x80000);
    const uint32_t p_heap_base = PORT_ARENA_LO + 0x4000;
    const uint32_t p_heap_end  = p_heap_base + 0x80000;

    int32_t o_heap;
    int p_heap;
    if (setup_one_heap(o_heap_base, o_heap_end, p_heap_base, p_heap_end, &o_heap, &p_heap) != 0) {
        fail_case(seed, run_idx, 0, alloc_only ? "OSAllocFromHeap" : "full", "heap setup failed");
        return 1;
    }

    for (int step = 0; step < steps; step++) {
        uint32_t r = xorshift32(&rng);
        int do_free = (!alloc_only) && (g_alloc_count > 0) && ((r & 3u) == 0u);

        if (!do_free) {
            uint32_t size = 1 + (xorshift32(&rng) % 0x2000u);
            if ((xorshift32(&rng) & 1u) == 0u) size &= ~31u;

            o32 o_ptr = oracle_OSAllocFromHeap(o_heap, size);
            void *p_ptr = OSAllocFromHeap(p_heap, size);

            if (compare_alloc_result(o_ptr, p_ptr, o_heap_base, p_heap_base) != 0) {
                fail_case(seed, run_idx, step, alloc_only ? "OSAllocFromHeap" : "full", "alloc return mismatch");
                return 1;
            }

            if (o_ptr != 0) {
                alloc_push(o_heap, o_ptr, p_ptr);
            }
        } else {
            int idx = alloc_pick_index(&rng);
            if (idx >= 0) {
                Alloc a = g_allocs[idx];
                oracle_OSFreeToHeap(a.heap, a.oracle_ptr);
                OSFreeToHeap(p_heap, a.port_ptr);
                alloc_remove_index(idx);
            }
        }

        int32_t o_free = oracle_OSCheckHeap(o_heap);
        long p_free = OSCheckHeap(p_heap);
        if ((long)o_free != p_free) {
            fail_case(seed, run_idx, step, alloc_only ? "OSAllocFromHeap" : "full", "OSCheckHeap free-bytes mismatch");
            return 1;
        }
    }
    return 0;
}

static int run_free_heavy_mix(uint32_t seed, int run_idx, int steps) {
    uint32_t rng = seed ? seed : 1;

    const o32 o_heap_base = 0x4000;
    const o32 o_heap_end  = (o32)(0x4000 + 0x80000);
    const uint32_t p_heap_base = PORT_ARENA_LO + 0x4000;
    const uint32_t p_heap_end  = p_heap_base + 0x80000;

    int32_t o_heap;
    int p_heap;
    if (setup_one_heap(o_heap_base, o_heap_end, p_heap_base, p_heap_end, &o_heap, &p_heap) != 0) {
        fail_case(seed, run_idx, 0, "OSFreeToHeap", "heap setup failed");
        return 1;
    }

    /* Pre-fill so frees exercise list/coalescing semantics. */
    for (int i = 0; i < 64; i++) {
        uint32_t size = 1 + (xorshift32(&rng) % 0x1800u);
        if ((xorshift32(&rng) & 1u) == 0u) size &= ~31u;

        o32 o_ptr = oracle_OSAllocFromHeap(o_heap, size);
        void *p_ptr = OSAllocFromHeap(p_heap, size);
        if (compare_alloc_result(o_ptr, p_ptr, o_heap_base, p_heap_base) != 0) {
            fail_case(seed, run_idx, i, "OSFreeToHeap", "prefill alloc return mismatch");
            return 1;
        }
        if (o_ptr == 0) break;
        alloc_push(o_heap, o_ptr, p_ptr);
    }

    for (int step = 0; step < steps; step++) {
        if (g_alloc_count <= 0) {
            /* Refill one allocation so we can keep freeing. */
            uint32_t size = 1 + (xorshift32(&rng) % 0x1000u);
            size = (size + 31u) & ~31u;
            o32 o_ptr = oracle_OSAllocFromHeap(o_heap, size);
            void *p_ptr = OSAllocFromHeap(p_heap, size);
            if (compare_alloc_result(o_ptr, p_ptr, o_heap_base, p_heap_base) != 0) {
                fail_case(seed, run_idx, step, "OSFreeToHeap", "refill alloc return mismatch");
                return 1;
            }
            if (o_ptr != 0) alloc_push(o_heap, o_ptr, p_ptr);
        } else {
            int idx = alloc_pick_index(&rng);
            if (idx >= 0) {
                Alloc a = g_allocs[idx];
                oracle_OSFreeToHeap(a.heap, a.oracle_ptr);
                OSFreeToHeap(p_heap, a.port_ptr);
                alloc_remove_index(idx);
            }
        }

        /* Keep heap moving: sometimes allocate again. */
        if ((xorshift32(&rng) & 3u) == 0u && g_alloc_count < (MAX_ALLOCS - 1)) {
            uint32_t size = 1 + (xorshift32(&rng) % 0x2000u);
            if ((xorshift32(&rng) & 1u) == 0u) size &= ~31u;
            o32 o_ptr = oracle_OSAllocFromHeap(o_heap, size);
            void *p_ptr = OSAllocFromHeap(p_heap, size);
            if (compare_alloc_result(o_ptr, p_ptr, o_heap_base, p_heap_base) != 0) {
                fail_case(seed, run_idx, step, "OSFreeToHeap", "alloc return mismatch");
                return 1;
            }
            if (o_ptr != 0) alloc_push(o_heap, o_ptr, p_ptr);
        }

        int32_t o_free = oracle_OSCheckHeap(o_heap);
        long p_free = OSCheckHeap(p_heap);
        if ((long)o_free != p_free) {
            fail_case(seed, run_idx, step, "OSFreeToHeap", "OSCheckHeap free-bytes mismatch");
            return 1;
        }
    }
    return 0;
}

static int test_OSDestroyHeap(uint32_t seed) {
    reset_all();

    if (oracle_OSInitAlloc(0x1000, (o32)(ORACLE_BUF_SIZE - 0x1000), 8) == 0) return 1;
    OSInitAlloc((void *)(uintptr_t)PORT_ARENA_LO, (void *)(uintptr_t)PORT_ARENA_HI, 8);

    /* Two heaps */
    const o32 o0_base = 0x4000, o0_end = 0x24000;
    const o32 o1_base = 0x28000, o1_end = 0x48000;
    int32_t o0 = oracle_OSCreateHeap(o0_base, o0_end);
    int32_t o1 = oracle_OSCreateHeap(o1_base, o1_end);

    const uint32_t p0_base = PORT_ARENA_LO + 0x4000, p0_end = PORT_ARENA_LO + 0x24000;
    const uint32_t p1_base = PORT_ARENA_LO + 0x28000, p1_end = PORT_ARENA_LO + 0x48000;
    int p0 = OSCreateHeap((void *)(uintptr_t)p0_base, (void *)(uintptr_t)p0_end);
    int p1 = OSCreateHeap((void *)(uintptr_t)p1_base, (void *)(uintptr_t)p1_end);

    if (o0 < 0 || o1 < 0 || p0 < 0 || p1 < 0) return 1;

    (void)oracle_OSAllocFromHeap(o0, 0x100);
    (void)OSAllocFromHeap(p0, 0x100);

    oracle_OSDestroyHeap(o0);
    OSDestroyHeap(p0);

    if (oracle_OSCheckHeap(o0) != -1) return 1;
    if (OSCheckHeap(p0) != -1) return 1;

    /* Other heap still fine */
    if (oracle_OSAllocFromHeap(o1, 0x80) == 0) return 1;
    if (OSAllocFromHeap(p1, 0x80) == NULL) return 1;
    if ((long)oracle_OSCheckHeap(o1) != OSCheckHeap(p1)) return 1;
    return 0;
}

static int test_OSAddToHeap(uint32_t seed) {
    (void)seed;
    reset_all();

    if (oracle_OSInitAlloc(0x1000, (o32)(ORACLE_BUF_SIZE - 0x1000), 8) == 0) return 1;
    OSInitAlloc((void *)(uintptr_t)PORT_ARENA_LO, (void *)(uintptr_t)PORT_ARENA_HI, 8);

    const o32 o_base = 0x4000;
    const uint32_t p_base = PORT_ARENA_LO + 0x4000;

    int32_t o_heap = oracle_OSCreateHeap(o_base, o_base + 0x4000);
    int p_heap = OSCreateHeap((void *)(uintptr_t)p_base, (void *)(uintptr_t)(p_base + 0x4000));
    if (o_heap < 0 || p_heap < 0) return 1;

    (void)oracle_OSAllocFromHeap(o_heap, 0x100);
    (void)OSAllocFromHeap(p_heap, 0x100);

    int32_t o_before = oracle_OSCheckHeap(o_heap);
    long p_before = OSCheckHeap(p_heap);
    if ((long)o_before != p_before) return 1;

    /* Add a disjoint region later in arena */
    oracle_OSAddToHeap(o_heap, o_base + 0x8000, o_base + 0xC000);
    OSAddToHeap(p_heap, (void *)(uintptr_t)(p_base + 0x8000), (void *)(uintptr_t)(p_base + 0xC000));

    int32_t o_after = oracle_OSCheckHeap(o_heap);
    long p_after = OSCheckHeap(p_heap);
    if ((long)o_after != p_after) return 1;
    if (o_after <= o_before) return 1;

    /* Allocation should still be consistent */
    o32 o_ptr = oracle_OSAllocFromHeap(o_heap, 0x800);
    void *p_ptr = OSAllocFromHeap(p_heap, 0x800);
    if (compare_alloc_result(o_ptr, p_ptr, o_base, p_base) != 0) return 1;
    return 0;
}

/* ── CLI ── */

static void usage(const char *argv0) {
    fprintf(stderr,
        "usage: %s [--op=NAME] [--seed=N] [--num-runs=N] [--steps=N] [-v]\n"
        "default: --op=full --num-runs=200 --steps=200\n"
        "\n"
        "ops:\n"
        "  DLAddFront  DLExtract  DLInsert  DLLookup\n"
        "  OSAllocFromHeap  OSFreeToHeap  OSDestroyHeap  OSAddToHeap\n"
        "  full\n",
        argv0);
}

int main(int argc, char **argv) {
    const char *op = "full";
    uint32_t seed = 0;
    int num_runs = 200;
    int steps = 200;
    int verbose = 0;

    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--op=", 5) == 0) {
            op = argv[i] + 5;
        } else if (strncmp(argv[i], "--seed=", 7) == 0) {
            seed = (uint32_t)strtoul(argv[i] + 7, NULL, 0);
        } else if (strncmp(argv[i], "--num-runs=", 11) == 0) {
            num_runs = (int)strtol(argv[i] + 11, NULL, 0);
        } else if (strncmp(argv[i], "--steps=", 8) == 0) {
            steps = (int)strtol(argv[i] + 8, NULL, 0);
        } else if (strcmp(argv[i], "-v") == 0) {
            verbose = 1;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 0;
        } else {
            usage(argv[0]);
            return 2;
        }
    }

    /* Determine run function */
    int (*fn_one)(uint32_t) = NULL;
    int run_with_steps = 0;
    int alloc_only = 0;
    int free_heavy = 0;

    if (strcmp(op, "DLAddFront") == 0) fn_one = test_DLAddFront;
    else if (strcmp(op, "DLExtract") == 0) fn_one = test_DLExtract;
    else if (strcmp(op, "DLInsert") == 0) fn_one = test_DLInsert;
    else if (strcmp(op, "DLLookup") == 0) fn_one = test_DLLookup;
    else if (strcmp(op, "OSDestroyHeap") == 0) fn_one = test_OSDestroyHeap;
    else if (strcmp(op, "OSAddToHeap") == 0) fn_one = test_OSAddToHeap;
    else if (strcmp(op, "OSAllocFromHeap") == 0) { run_with_steps = 1; alloc_only = 1; }
    else if (strcmp(op, "OSFreeToHeap") == 0) { run_with_steps = 1; free_heavy = 1; }
    else if (strcmp(op, "full") == 0) { run_with_steps = 1; alloc_only = 0; }
    else {
        fprintf(stderr, "unknown --op=%s\n", op);
        usage(argv[0]);
        return 2;
    }

    if (seed != 0) {
        int rc;
        if (!run_with_steps) rc = fn_one(seed);
        else if (free_heavy) rc = run_free_heavy_mix(seed, 0, steps);
        else rc = run_alloc_free_mix(seed, 0, steps, alloc_only);
        if (rc == 0 && verbose) fprintf(stderr, "[OK] seed=0x%08X\n", seed);
        return rc;
    }

    for (int i = 0; i < num_runs; i++) {
        uint32_t s = 0x9E3779B9u ^ (uint32_t)i * 0x85EBCA6Bu;
        int rc;
        if (!run_with_steps) rc = fn_one(s);
        else if (free_heavy) rc = run_free_heavy_mix(s, i, steps);
        else rc = run_alloc_free_mix(s, i, steps, alloc_only);
        if (rc != 0) return rc;
        if (verbose && ((i % 50) == 0)) fprintf(stderr, "[OK] %d/%d\n", i, num_runs);
    }

    if (verbose) fprintf(stderr, "[OK] all runs\n");
    return 0;
}
