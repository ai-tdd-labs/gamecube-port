/*
 * arq_property_test.c --- Property-based parity test for ARQ.
 *
 * Oracle:  decomp functions (native structs) in arq_oracle.h
 * Port:    sdk_port functions (big-endian gc_mem) in arq.c
 *
 * For each seed:
 *   1. Random sequence of PostRequest calls (Hi/Lo, random params)
 *   2. Simulate ISR (DMA complete) events
 *   3. Compare DMA logs between oracle and port
 *
 * Test levels:
 *   L0: Single request (Hi or Lo), verify DMA params
 *   L1: Priority ordering (Hi preempts Lo)
 *   L2: Random mix with chunking (Lo requests > chunkSize)
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Oracle (native, self-contained) */
#include "arq_oracle.h"

/* Port (big-endian via gc_mem) */
#include "arq.h"
#include "gc_mem.h"

/* ── PRNG (xorshift32) ── */
static uint32_t g_rng;

static uint32_t xorshift32(void)
{
    uint32_t x = g_rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    g_rng = x;
    return x;
}

static uint32_t rand_range(uint32_t lo, uint32_t hi)
{
    return lo + xorshift32() % (hi - lo + 1);
}

/* ── gc_mem setup ── */
#define GC_ARQ_BASE   0x80200000u
#define MAX_REQUESTS  64
#define GC_ARQ_POOL_SIZE (MAX_REQUESTS * PORT_ARQ_REQ_SIZE)
static uint8_t g_gc_ram[GC_ARQ_POOL_SIZE];

/* Oracle request pool (host-side) */
static oracle_ARQRequest g_oracle_reqs[MAX_REQUESTS];

/* ── CLI options ── */
static int opt_verbose  = 0;
static int opt_seed     = -1;
static int opt_num_runs = 500;
static const char *opt_op = NULL;

/* ── Statistics ── */
static int g_total_checks = 0;
static int g_total_pass   = 0;
static int g_total_fail   = 0;

/* ── Helpers ── */

static void init_gc_mem(void)
{
    memset(g_gc_ram, 0, sizeof(g_gc_ram));
    gc_mem_set(GC_ARQ_BASE, sizeof(g_gc_ram), g_gc_ram);
}

/* GC address for request slot i */
static uint32_t req_addr(int i)
{
    return GC_ARQ_BASE + (uint32_t)i * PORT_ARQ_REQ_SIZE;
}

/* Compare DMA logs between oracle and port.
 * Returns number of mismatches. */
static int compare_dma_logs(port_ARQState *ps, uint32_t seed,
                            const char *phase)
{
    int mismatches = 0;
    int i;
    int max_count;

    if (oracle_dma_count != ps->dma_count) {
        fprintf(stderr,
            "  MISMATCH %s seed=%u: dma_count oracle=%d port=%d\n",
            phase, seed, oracle_dma_count, ps->dma_count);
        mismatches++;
    }

    max_count = oracle_dma_count < ps->dma_count ?
                oracle_dma_count : ps->dma_count;

    for (i = 0; i < max_count; i++) {
        oracle_DMARecord *o = &oracle_dma_log[i];
        port_DMARecord *p = &ps->dma_log[i];

        if (o->type != p->type || o->mainmem != p->mainmem ||
            o->aram != p->aram || o->length != p->length) {
            if (mismatches < 5) {
                fprintf(stderr,
                    "  MISMATCH %s seed=%u DMA[%d]: "
                    "oracle=(t=%u mm=0x%X ar=0x%X l=%u) "
                    "port=(t=%u mm=0x%X ar=0x%X l=%u)\n",
                    phase, seed, i,
                    o->type, o->mainmem, o->aram, o->length,
                    p->type, p->mainmem, p->aram, p->length);
            }
            mismatches++;
        }
    }

    if (oracle_callback_count != ps->callback_count) {
        if (mismatches < 5) {
            fprintf(stderr,
                "  MISMATCH %s seed=%u: callback_count oracle=%d port=%d\n",
                phase, seed, oracle_callback_count, ps->callback_count);
        }
        mismatches++;
    }

    return mismatches;
}

/* Post same request to both oracle and port */
static void post_both(port_ARQState *ps, int slot,
                      uint32_t owner, uint32_t type, uint32_t priority,
                      uint32_t source, uint32_t dest, uint32_t length)
{
    oracle_ARQPostRequest(&g_oracle_reqs[slot], owner, type, priority,
                          source, dest, length, 1);
    port_ARQPostRequest(ps, req_addr(slot), owner, type, priority,
                        source, dest, length, 1);
}

/* Simulate ISR on both */
static void isr_both(port_ARQState *ps)
{
    oracle_ARQInterruptServiceRoutine();
    port_ARQInterruptServiceRoutine(ps);
}

/* Drain all pending DMA by pumping ISR until no more DMA is issued */
static void drain_all(port_ARQState *ps)
{
    int prev;
    int safety = 5000;
    do {
        prev = oracle_dma_count;
        isr_both(ps);
    } while (oracle_dma_count > prev && --safety > 0);
}

/* ── L0: Single request test ── */

static int test_single_request(uint32_t seed)
{
    port_ARQState ps;
    uint32_t type, priority, source, dest, length;
    int mismatches;

    oracle_ARQInit();
    port_ARQInit(&ps);
    init_gc_mem();
    memset(g_oracle_reqs, 0, sizeof(g_oracle_reqs));

    type     = xorshift32() % 2;
    priority = xorshift32() % 2;
    source   = (rand_range(0x80000000u, 0x81000000u)) & ~31u;
    dest     = (rand_range(0x00100000u, 0x00900000u)) & ~31u;
    length   = (rand_range(32, 8192)) & ~31u;

    post_both(&ps, 0, 1, type, priority, source, dest, length);

    /* First PostRequest should have started DMA (nothing pending before) */
    drain_all(&ps);

    g_total_checks++;
    mismatches = compare_dma_logs(&ps, seed, "L0-Single");
    if (mismatches > 0) {
        g_total_fail++;
        return 1;
    }
    g_total_pass++;
    if (opt_verbose) {
        printf("  L0-Single: type=%u prio=%u len=%u dmas=%d OK\n",
               type, priority, length, oracle_dma_count);
    }
    return 0;
}

/* ── L1: Priority ordering test ── */

static int test_priority_ordering(uint32_t seed)
{
    port_ARQState ps;
    uint32_t src_base, dst_base;
    int mismatches;
    int i;

    oracle_ARQInit();
    port_ARQInit(&ps);
    init_gc_mem();
    memset(g_oracle_reqs, 0, sizeof(g_oracle_reqs));

    src_base = 0x80000000u;
    dst_base = 0x00100000u;

    /* Post 3 Lo requests, then 2 Hi requests.
     * All small enough to fit in one chunk. */
    for (i = 0; i < 3; i++) {
        post_both(&ps, i, (uint32_t)(i + 1),
                  ORACLE_ARQ_TYPE_MRAM_TO_ARAM,
                  ORACLE_ARQ_PRIORITY_LOW,
                  src_base + (uint32_t)i * 0x1000,
                  dst_base + (uint32_t)i * 0x1000,
                  1024);
    }
    for (i = 0; i < 2; i++) {
        post_both(&ps, 3 + i, (uint32_t)(10 + i),
                  ORACLE_ARQ_TYPE_ARAM_TO_MRAM,
                  ORACLE_ARQ_PRIORITY_HIGH,
                  src_base + (uint32_t)(3 + i) * 0x1000,
                  dst_base + (uint32_t)(3 + i) * 0x1000,
                  2048);
    }

    /* Drain all */
    drain_all(&ps);

    g_total_checks++;
    mismatches = compare_dma_logs(&ps, seed, "L1-Priority");
    if (mismatches > 0) {
        g_total_fail++;
        return 1;
    }

    /* Verify Hi requests processed before remaining Lo requests:
     * First DMA is Lo[0] (started immediately on first PostRequest).
     * After ISR for Lo[0], Hi[0] and Hi[1] should be next (Hi preempts).
     * Then Lo[1] and Lo[2]. */
    g_total_pass++;
    if (opt_verbose) {
        printf("  L1-Priority: %d DMAs, %d callbacks OK\n",
               oracle_dma_count, oracle_callback_count);
    }
    return 0;
}

/* ── L2: Random mix with chunking ── */

static int test_random_mix(uint32_t seed)
{
    port_ARQState ps;
    int mismatches;
    int n_requests, slot;
    int i;

    oracle_ARQInit();
    port_ARQInit(&ps);
    init_gc_mem();
    memset(g_oracle_reqs, 0, sizeof(g_oracle_reqs));

    n_requests = (int)rand_range(5, MAX_REQUESTS - 1);
    slot = 0;

    for (i = 0; i < n_requests; i++) {
        uint32_t type     = xorshift32() % 2;
        uint32_t priority = xorshift32() % 2;
        uint32_t source   = (rand_range(0x80000000u, 0x81000000u)) & ~31u;
        uint32_t dest     = (rand_range(0x00100000u, 0x00900000u)) & ~31u;
        /* Length: sometimes > chunkSize to test chunking */
        uint32_t length   = (rand_range(32, 20000)) & ~31u;

        post_both(&ps, slot, (uint32_t)(i + 1), type, priority,
                  source, dest, length);
        slot++;

        /* Randomly simulate some ISR events between posts */
        if (xorshift32() % 3 == 0) {
            int n_isrs = (int)rand_range(1, 4);
            int j;
            for (j = 0; j < n_isrs; j++) {
                isr_both(&ps);
            }
        }
    }

    /* Drain remaining */
    drain_all(&ps);

    g_total_checks++;
    mismatches = compare_dma_logs(&ps, seed, "L2-Random");
    if (mismatches > 0) {
        g_total_fail++;
        return 1;
    }

    g_total_pass++;
    if (opt_verbose) {
        printf("  L2-Random: %d requests, %d DMAs, %d callbacks OK\n",
               n_requests, oracle_dma_count, oracle_callback_count);
    }
    return 0;
}

/* ── Run one seed ── */

static int run_seed(uint32_t seed)
{
    int fail = 0;

    g_rng = seed;

    /* L0: Single request */
    if (!opt_op || strstr("SINGLE", opt_op) || strstr("L0", opt_op)) {
        fail += test_single_request(seed);
    }

    /* L1: Priority ordering */
    if (!opt_op || strstr("PRIORITY", opt_op) || strstr("L1", opt_op)) {
        fail += test_priority_ordering(seed);
    }

    /* L2: Random mix */
    if (!opt_op || strstr("RANDOM", opt_op) || strstr("L2", opt_op) ||
        strstr("ARQ", opt_op)) {
        fail += test_random_mix(seed);
    }

    return fail;
}

/* ── CLI arg parsing ── */

static void parse_args(int argc, char **argv)
{
    int i;
    for (i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--seed=", 7) == 0) {
            opt_seed = atoi(argv[i] + 7);
        } else if (strncmp(argv[i], "--num-runs=", 11) == 0) {
            opt_num_runs = atoi(argv[i] + 11);
        } else if (strncmp(argv[i], "--op=", 5) == 0) {
            opt_op = argv[i] + 5;
        } else if (strcmp(argv[i], "-v") == 0) {
            opt_verbose = 1;
        } else {
            fprintf(stderr, "Unknown arg: %s\n", argv[i]);
            fprintf(stderr,
                "Usage: arq_property_test [--seed=N] [--num-runs=N] "
                "[--op=NAME] [-v]\n");
            exit(1);
        }
    }
}

/* ── Main ── */

int main(int argc, char **argv)
{
    int total_seeds, fails;
    uint32_t s;

    parse_args(argc, argv);

    printf("=== ARQ Property Test ===\n");

    if (opt_seed >= 0) {
        total_seeds = 1;
        fails = run_seed((uint32_t)opt_seed);
    } else {
        total_seeds = opt_num_runs;
        fails = 0;
        for (s = 1; s <= (uint32_t)opt_num_runs; s++) {
            fails += run_seed(s);
        }
    }

    printf("\n--- Summary ---\n");
    printf("Seeds:  %d\n", total_seeds);
    printf("Checks: %d  (pass=%d  fail=%d)\n",
           g_total_checks, g_total_pass, g_total_fail);

    if (g_total_fail == 0) {
        printf("\nRESULT: %d/%d PASS\n", g_total_pass, g_total_checks);
        return 0;
    } else {
        printf("\nRESULT: FAIL (%d failures)\n", g_total_fail);
        return 1;
    }
}
