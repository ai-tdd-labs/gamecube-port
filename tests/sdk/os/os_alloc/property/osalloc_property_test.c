/*
 * osalloc_property_test.c — Property-style parity test for OSAlloc (oracle vs port).
 *
 * Portable (no -m32):
 * - Oracle uses 32-bit offset pointers in an arena buffer.
 * - Port uses sdk_port OSAlloc + emulated big-endian RAM (gc_mem).
 *
 * This is a "1-button loop": run many random alloc/free sequences under a seed
 * and assert semantic parity after each operation:
 * - return offsets (relative to heap base)
 * - OSCheckHeap free-byte count
 *
 * Notes:
 * - This does NOT try to be a Dolphin RAM-dump oracle. It's a fast host-only
 *   convergence tool for allocator semantics.
 * - For full bit-exact memory layout and hardware-ish behavior, use DOL tests.
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

typedef struct {
    int heap;
    o32 oracle_ptr; /* offset pointer (header already applied) */
    void *port_ptr; /* GC-address-as-pointer */
    uint32_t size;
} Alloc;

static Alloc g_allocs[MAX_ALLOCS];
static int g_alloc_count;

static void alloc_reset(void) {
    g_alloc_count = 0;
    memset(g_allocs, 0, sizeof(g_allocs));
}

static void alloc_push(int heap, o32 o_ptr, void *p_ptr, uint32_t size) {
    if (g_alloc_count >= MAX_ALLOCS) return;
    g_allocs[g_alloc_count].heap = heap;
    g_allocs[g_alloc_count].oracle_ptr = o_ptr;
    g_allocs[g_alloc_count].port_ptr = p_ptr;
    g_allocs[g_alloc_count].size = size;
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

static void fail_case(uint32_t seed, int run_idx, int step_idx, const char *msg) {
    fprintf(stderr, "[FAIL] seed=0x%08X run=%d step=%d %s\n", seed, run_idx, step_idx, msg);
}

static int32_t oracle_setup_one_heap(o32 heap_start, o32 heap_end) {
    if (oracle_OSInitAlloc(0x1000, (o32)(ORACLE_BUF_SIZE - 0x1000), 8) == 0) return -1;
    int32_t heap = oracle_OSCreateHeap(heap_start, heap_end);
    oracle_OSSetCurrentHeap(heap);
    return heap;
}

static int port_setup_one_heap(uint32_t heap_start, uint32_t heap_end) {
    OSInitAlloc((void *)(uintptr_t)PORT_ARENA_LO, (void *)(uintptr_t)PORT_ARENA_HI, 8);
    int heap = OSCreateHeap((void *)(uintptr_t)heap_start, (void *)(uintptr_t)heap_end);
    OSSetCurrentHeap(heap);
    return heap;
}

static int compare_alloc_result(o32 oracle_ptr, void *port_ptr, o32 oracle_heap_base, uint32_t port_heap_base) {
    if ((oracle_ptr == 0) != (port_ptr == NULL)) return 1;
    if (oracle_ptr == 0) return 0;

    uint32_t o_rel = (uint32_t)(oracle_ptr - oracle_heap_base);
    uint32_t p_rel = (uint32_t)((uint32_t)(uintptr_t)port_ptr - port_heap_base);
    return o_rel != p_rel;
}

static int run_one_seed(uint32_t seed, int run_idx, int steps) {
    uint32_t rng = seed ? seed : 1;
    reset_all();
    alloc_reset();

    /* Choose a heap window (aligned-ish). */
    o32 o_heap_base = 0x4000;
    o32 o_heap_end  = (o32)(0x4000 + 0x80000); /* 512 KiB */

    uint32_t p_heap_base = PORT_ARENA_LO + 0x4000;
    uint32_t p_heap_end  = p_heap_base + 0x80000;

    int32_t o_heap = oracle_setup_one_heap(o_heap_base, o_heap_end);
    int p_heap = port_setup_one_heap(p_heap_base, p_heap_end);
    if (o_heap < 0 || p_heap < 0) {
        fail_case(seed, run_idx, 0, "heap setup failed");
        return 1;
    }

    for (int step = 0; step < steps; step++) {
        uint32_t r = xorshift32(&rng);
        int do_free = (g_alloc_count > 0) && ((r & 3u) == 0u); /* 25% frees */

        if (!do_free) {
            uint32_t size = 1 + (xorshift32(&rng) % 0x2000u);
            if ((xorshift32(&rng) & 1u) == 0u) size &= ~31u; /* bias to aligned sizes */

            o32 o_ptr = oracle_OSAllocFromHeap(o_heap, size);
            void *p_ptr = OSAllocFromHeap(p_heap, size);

            if (compare_alloc_result(o_ptr, p_ptr, o_heap_base, p_heap_base) != 0) {
                fail_case(seed, run_idx, step, "alloc return mismatch");
                return 1;
            }

            if (o_ptr != 0) {
                alloc_push(o_heap, o_ptr, p_ptr, size);
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
            fail_case(seed, run_idx, step, "OSCheckHeap free-bytes mismatch");
            return 1;
        }
    }

    return 0;
}

static void usage(const char *argv0) {
    fprintf(stderr,
        "usage: %s [--seed=N] [--num-runs=N] [--steps=N] [-v]\n"
        "default: --num-runs=200 --steps=200\n",
        argv0);
}

int main(int argc, char **argv) {
    uint32_t seed = 0;
    int num_runs = 200;
    int steps = 200;
    int verbose = 0;

    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--seed=", 7) == 0) {
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

    if (seed != 0) {
        int rc = run_one_seed(seed, 0, steps);
        if (rc == 0 && verbose) {
            fprintf(stderr, "[OK] seed=0x%08X\n", seed);
        }
        return rc;
    }

    for (int i = 0; i < num_runs; i++) {
        uint32_t s = 0x9E3779B9u ^ (uint32_t)i * 0x85EBCA6Bu;
        int rc = run_one_seed(s, i, steps);
        if (rc != 0) return rc;
        if (verbose && ((i % 50) == 0)) {
            fprintf(stderr, "[OK] %d/%d\n", i, num_runs);
        }
    }

    if (verbose) fprintf(stderr, "[OK] all runs\n");
    return 0;
}

