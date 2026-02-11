/*
 * stopwatch_property_test.c --- Property-based parity test for OSStopwatch.
 *
 * Oracle:  decomp OSStopwatch (inline, oracle_ prefix)
 * Port:    sdk_port OSStopwatch (inline, port_ prefix)
 *
 * Both share a single mock time counter (g_mock_time) controlled by the test.
 * For each seed, generates random sequences of Init/Start/Stop/Check/Reset
 * and compares all fields after each operation.
 *
 * Source of truth:
 *   external/mp4-decomp/src/dolphin/os/OSStopwatch.c
 *   src/sdk_port/os/OSStopwatch.c
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Shared mock time ── */
static uint64_t g_mock_time;

/* ════════════════════════════════════════════════════════════════════
 * Oracle (exact decomp copy with oracle_ prefix)
 * ════════════════════════════════════════════════════════════════════ */

typedef struct {
    char    *name;
    uint64_t total;
    uint32_t hits;
    uint32_t running;
    uint64_t last;
    uint64_t min;
    uint64_t max;
} oracle_OSStopwatch;

static uint64_t oracle_OSGetTime(void) { return g_mock_time; }

static void oracle_OSInitStopwatch(oracle_OSStopwatch *sw, char *name)
{
    sw->name = name;
    sw->total = 0;
    sw->hits = 0;
    sw->min = 0x00000000FFFFFFFFull;
    sw->max = 0;
    sw->running = 0;
    sw->last = 0;
}

static void oracle_OSStartStopwatch(oracle_OSStopwatch *sw)
{
    sw->running = 1;
    sw->last = oracle_OSGetTime();
}

static void oracle_OSStopStopwatch(oracle_OSStopwatch *sw)
{
    long long interval;
    if (sw->running != 0) {
        interval = (long long)(oracle_OSGetTime() - sw->last);
        sw->total += (uint64_t)interval;
        sw->running = 0;
        sw->hits++;
        if (sw->max < (uint64_t)interval) {
            sw->max = (uint64_t)interval;
        }
        if ((uint64_t)interval < sw->min) {
            sw->min = (uint64_t)interval;
        }
    }
}

static long long oracle_OSCheckStopwatch(oracle_OSStopwatch *sw)
{
    long long currTotal;
    currTotal = (long long)sw->total;
    if (sw->running != 0) {
        currTotal += (long long)(oracle_OSGetTime() - sw->last);
    }
    return currTotal;
}

static void oracle_OSResetStopwatch(oracle_OSStopwatch *sw)
{
    oracle_OSInitStopwatch(sw, sw->name);
}

/* ════════════════════════════════════════════════════════════════════
 * Port (exact copy of sdk_port version with port_ prefix)
 * ════════════════════════════════════════════════════════════════════ */

typedef struct {
    char    *name;
    uint64_t total;
    uint32_t hits;
    uint32_t running;
    uint64_t last;
    uint64_t min;
    uint64_t max;
} port_OSStopwatch;

static uint64_t port_OSGetTime(void) { return g_mock_time; }

static void port_OSInitStopwatch(port_OSStopwatch *sw, char *name)
{
    sw->name = name;
    sw->total = 0;
    sw->hits = 0;
    sw->min = 0x00000000FFFFFFFFull;
    sw->max = 0;
    sw->running = 0;
    sw->last = 0;
}

static void port_OSStartStopwatch(port_OSStopwatch *sw)
{
    sw->running = 1;
    sw->last = port_OSGetTime();
}

static void port_OSStopStopwatch(port_OSStopwatch *sw)
{
    long long interval;
    if (sw->running != 0) {
        interval = (long long)(port_OSGetTime() - sw->last);
        sw->total += (uint64_t)interval;
        sw->running = 0;
        sw->hits++;
        if (sw->max < (uint64_t)interval) {
            sw->max = (uint64_t)interval;
        }
        if ((uint64_t)interval < sw->min) {
            sw->min = (uint64_t)interval;
        }
    }
}

static long long port_OSCheckStopwatch(port_OSStopwatch *sw)
{
    long long currTotal;
    currTotal = (long long)sw->total;
    if (sw->running != 0) {
        currTotal += (long long)(port_OSGetTime() - sw->last);
    }
    return currTotal;
}

static void port_OSResetStopwatch(port_OSStopwatch *sw)
{
    port_OSInitStopwatch(sw, sw->name);
}

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

/* ── Statistics ── */
static int g_total_checks = 0;
static int g_total_pass   = 0;
static int g_total_fail   = 0;
static int opt_verbose = 0;
static const char *opt_op = NULL;

/* ── State comparison ── */

static int compare_stopwatch(const char *op, uint32_t seed,
                             oracle_OSStopwatch *o, port_OSStopwatch *p)
{
    int mismatches = 0;

    if (o->total != p->total) {
        if (mismatches < 5)
            fprintf(stderr, "  MISMATCH %s seed=%u: total oracle=%llu port=%llu\n",
                    op, seed, (unsigned long long)o->total, (unsigned long long)p->total);
        mismatches++;
    }
    if (o->hits != p->hits) {
        if (mismatches < 5)
            fprintf(stderr, "  MISMATCH %s seed=%u: hits oracle=%u port=%u\n",
                    op, seed, o->hits, p->hits);
        mismatches++;
    }
    if (o->running != p->running) {
        if (mismatches < 5)
            fprintf(stderr, "  MISMATCH %s seed=%u: running oracle=%u port=%u\n",
                    op, seed, o->running, p->running);
        mismatches++;
    }
    if (o->last != p->last) {
        if (mismatches < 5)
            fprintf(stderr, "  MISMATCH %s seed=%u: last oracle=%llu port=%llu\n",
                    op, seed, (unsigned long long)o->last, (unsigned long long)p->last);
        mismatches++;
    }
    if (o->min != p->min) {
        if (mismatches < 5)
            fprintf(stderr, "  MISMATCH %s seed=%u: min oracle=%llu port=%llu\n",
                    op, seed, (unsigned long long)o->min, (unsigned long long)p->min);
        mismatches++;
    }
    if (o->max != p->max) {
        if (mismatches < 5)
            fprintf(stderr, "  MISMATCH %s seed=%u: max oracle=%llu port=%llu\n",
                    op, seed, (unsigned long long)o->max, (unsigned long long)p->max);
        mismatches++;
    }

    g_total_checks++;
    if (mismatches > 0) {
        g_total_fail++;
        return 1;
    }
    g_total_pass++;
    return 0;
}

/* ── Test: random stopwatch operations ── */

static int test_stopwatch_random(uint32_t seed)
{
    oracle_OSStopwatch osw;
    port_OSStopwatch psw;
    int fail = 0;
    int n_ops;
    int op;
    static char name[] = "test";

    g_mock_time = 1000; /* start at a non-zero time */

    oracle_OSInitStopwatch(&osw, name);
    port_OSInitStopwatch(&psw, name);
    fail += compare_stopwatch("Init", seed, &osw, &psw);
    if (fail) return fail;

    n_ops = (int)rand_range(20, 80);

    for (op = 0; op < n_ops && fail == 0; op++) {
        uint32_t r = xorshift32() % 100;

        /* Advance time by a random delta (1-100) */
        g_mock_time += rand_range(1, 100);

        if (r < 25) {
            /* Start */
            oracle_OSStartStopwatch(&osw);
            port_OSStartStopwatch(&psw);
            fail += compare_stopwatch("Start", seed, &osw, &psw);
        } else if (r < 50) {
            /* Stop */
            oracle_OSStopStopwatch(&osw);
            port_OSStopStopwatch(&psw);
            fail += compare_stopwatch("Stop", seed, &osw, &psw);
        } else if (r < 75) {
            /* Check */
            long long oc = oracle_OSCheckStopwatch(&osw);
            long long pc = port_OSCheckStopwatch(&psw);
            if (oc != pc) {
                fprintf(stderr, "  MISMATCH Check seed=%u: oracle=%lld port=%lld\n",
                        seed, oc, pc);
                g_total_fail++;
                g_total_checks++;
                return 1;
            }
            fail += compare_stopwatch("Check", seed, &osw, &psw);
        } else if (r < 90) {
            /* Reset */
            oracle_OSResetStopwatch(&osw);
            port_OSResetStopwatch(&psw);
            fail += compare_stopwatch("Reset", seed, &osw, &psw);
        } else {
            /* Re-Init with different name */
            oracle_OSInitStopwatch(&osw, name);
            port_OSInitStopwatch(&psw, name);
            fail += compare_stopwatch("ReInit", seed, &osw, &psw);
        }
    }

    if (opt_verbose && fail == 0) {
        printf("  Stopwatch: %d ops OK\n", n_ops);
    }

    return fail;
}

/* ── Test: multiple stopwatches interleaved ── */

static int test_stopwatch_multi(uint32_t seed)
{
    oracle_OSStopwatch osw[4];
    port_OSStopwatch psw[4];
    int fail = 0;
    int n_ops, op, i;
    static char *names[] = {"sw0", "sw1", "sw2", "sw3"};

    g_mock_time = 500;

    for (i = 0; i < 4; i++) {
        oracle_OSInitStopwatch(&osw[i], names[i]);
        port_OSInitStopwatch(&psw[i], names[i]);
    }

    n_ops = (int)rand_range(30, 100);

    for (op = 0; op < n_ops && fail == 0; op++) {
        int sw_idx = (int)(xorshift32() % 4);
        uint32_t r = xorshift32() % 100;

        g_mock_time += rand_range(1, 50);

        if (r < 30) {
            oracle_OSStartStopwatch(&osw[sw_idx]);
            port_OSStartStopwatch(&psw[sw_idx]);
        } else if (r < 60) {
            oracle_OSStopStopwatch(&osw[sw_idx]);
            port_OSStopStopwatch(&psw[sw_idx]);
        } else if (r < 80) {
            long long oc = oracle_OSCheckStopwatch(&osw[sw_idx]);
            long long pc = port_OSCheckStopwatch(&psw[sw_idx]);
            if (oc != pc) {
                fprintf(stderr, "  MISMATCH Multi-Check seed=%u sw=%d: oracle=%lld port=%lld\n",
                        seed, sw_idx, oc, pc);
                g_total_fail++;
                g_total_checks++;
                return 1;
            }
        } else if (r < 95) {
            oracle_OSResetStopwatch(&osw[sw_idx]);
            port_OSResetStopwatch(&psw[sw_idx]);
        } else {
            oracle_OSInitStopwatch(&osw[sw_idx], names[sw_idx]);
            port_OSInitStopwatch(&psw[sw_idx], names[sw_idx]);
        }

        /* Compare all 4 stopwatches */
        for (i = 0; i < 4 && fail == 0; i++) {
            char label[32];
            snprintf(label, sizeof(label), "Multi-sw%d", i);
            fail += compare_stopwatch(label, seed, &osw[i], &psw[i]);
        }
    }

    if (opt_verbose && fail == 0) {
        printf("  Multi-Stopwatch: %d ops OK\n", n_ops);
    }

    return fail;
}

/* ── Run one seed ── */

static int run_seed(uint32_t seed)
{
    int fail = 0;

    /* L0: Random single stopwatch */
    if (!opt_op || strstr("L0", opt_op) || strstr("RANDOM", opt_op) || strstr("SINGLE", opt_op)) {
        g_rng = seed;
        fail += test_stopwatch_random(seed);
    }

    /* L1: Multiple stopwatches interleaved */
    if (!opt_op || strstr("L1", opt_op) || strstr("MULTI", opt_op)) {
        g_rng = seed ^ 0xABCD1234u;
        if (g_rng == 0) g_rng = 1;
        fail += test_stopwatch_multi(seed);
    }

    return fail;
}

/* ── CLI arg parsing ── */

static void parse_args(int argc, char **argv, int *opt_seed, int *opt_num_runs)
{
    int i;
    for (i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--seed=", 7) == 0) {
            *opt_seed = atoi(argv[i] + 7);
        } else if (strncmp(argv[i], "--num-runs=", 11) == 0) {
            *opt_num_runs = atoi(argv[i] + 11);
        } else if (strncmp(argv[i], "--op=", 5) == 0) {
            opt_op = argv[i] + 5;
        } else if (strcmp(argv[i], "-v") == 0) {
            opt_verbose = 1;
        } else {
            fprintf(stderr, "Unknown arg: %s\n", argv[i]);
            fprintf(stderr,
                "Usage: stopwatch_property_test [--seed=N] [--num-runs=N] [--op=L0|L1|RANDOM|MULTI] [-v]\n");
            exit(1);
        }
    }
}

/* ── Main ── */

int main(int argc, char **argv)
{
    int opt_seed = -1;
    int opt_num_runs = 500;
    int total_seeds, fails;
    uint32_t s;

    parse_args(argc, argv, &opt_seed, &opt_num_runs);

    printf("=== OSStopwatch Property Test ===\n");

    if (opt_seed >= 0) {
        total_seeds = 1;
        fails = run_seed((uint32_t)opt_seed);
    } else {
        total_seeds = opt_num_runs;
        fails = 0;
        for (s = 1; s <= (uint32_t)opt_num_runs; s++) {
            if (s % 100 == 0 || opt_verbose)
                printf("  progress: seed %u/%d\n", s, opt_num_runs);
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
