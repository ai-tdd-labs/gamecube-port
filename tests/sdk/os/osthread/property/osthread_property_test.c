/*
 * osthread_property_test.c --- Property-based parity test for OSThread.
 *
 * Oracle:  decomp scheduler (native structs) in osthread_oracle.h
 * Port:    sdk_port scheduler (big-endian gc_mem) in osthread.c
 *
 * For each seed:
 *   1. Random thread set (different priorities, some detached)
 *   2. Random mix of Resume/Suspend/Sleep/Wakeup/Yield/Cancel
 *   3. After each operation: compare observable state
 *
 * Test levels:
 *   L0: Basic lifecycle (create, resume, verify scheduling)
 *   L1: Suspend/Resume cycles with priority preemption
 *   L2: Sleep/Wakeup with wait queues
 *   L3: Random integration (all operations)
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Oracle (native, self-contained) */
#include "osthread_oracle.h"

/* Port (big-endian via gc_mem) */
#include "osthread.h"
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

/* ── gc_mem backing buffer ── */
#define GC_BASE  0x80300000u
static uint8_t g_gc_ram[PORT_TOTAL_SIZE];

/* Port state */
static port_OSThreadState g_ps;

/* ── gc_mem readers (for comparison) ── */

static uint16_t gc_load_u16(uint32_t addr)
{
    uint8_t *p = gc_mem_ptr(addr, 2);
    return (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

static uint32_t gc_load_u32(uint32_t addr)
{
    uint8_t *p = gc_mem_ptr(addr, 4);
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8)  | (uint32_t)p[3];
}

static int32_t gc_load_s32(uint32_t addr)
{
    return (int32_t)gc_load_u32(addr);
}

/* ── Thread tracking ── */
#define MAX_TEST_THREADS 16
#define MAX_TEST_WAITQ   8
static int g_thread_created[MAX_TEST_THREADS];
static int g_num_created;

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
    gc_mem_set(GC_BASE, sizeof(g_gc_ram), g_gc_ram);
}

static void init_both(void)
{
    init_gc_mem();
    oracle_OSThreadInit();
    port_OSThreadInit(&g_ps, GC_BASE);
    memset(g_thread_created, 0, sizeof(g_thread_created));
    g_thread_created[0] = 1; /* default thread */
    g_num_created = 1;
}

static void create_both(int slot, int32_t priority, uint16_t attr)
{
    oracle_OSCreateThread(&oracle_ThreadPool[slot], priority, attr);
    port_OSCreateThread(&g_ps, slot, priority, attr);
    g_thread_created[slot] = 1;
    g_num_created++;
}

/* ── State comparison ── */

static int compare_state(const char *op, uint32_t seed)
{
    int mismatches = 0;
    int i;

    /* Current thread */
    int o_cur = oracle_CurrentThread ? oracle_CurrentThread->id : -1;
    int p_cur = -1;
    if (g_ps.currentThread != 0) {
        p_cur = (int)((g_ps.currentThread - g_ps.gc_base -
                        PORT_THREADS_OFFSET) / PORT_THREAD_SIZE);
    }
    if (o_cur != p_cur) {
        if (mismatches < 5)
            fprintf(stderr,
                "  MISMATCH %s seed=%u: currentThread oracle=%d port=%d\n",
                op, seed, o_cur, p_cur);
        mismatches++;
    }

    /* RunQueueBits */
    if (oracle_RunQueueBits != g_ps.runQueueBits) {
        if (mismatches < 5)
            fprintf(stderr,
                "  MISMATCH %s seed=%u: RunQueueBits oracle=0x%08X port=0x%08X\n",
                op, seed, oracle_RunQueueBits, g_ps.runQueueBits);
        mismatches++;
    }

    /* Reschedule counter */
    if (oracle_Reschedule != g_ps.reschedule) {
        if (mismatches < 5)
            fprintf(stderr,
                "  MISMATCH %s seed=%u: Reschedule oracle=%d port=%d\n",
                op, seed, oracle_Reschedule, g_ps.reschedule);
        mismatches++;
    }

    /* Per-thread state */
    for (i = 0; i < MAX_TEST_THREADS; i++) {
        oracle_OSThread *ot;
        uint32_t pt;
        uint16_t o_state, p_state;
        int32_t o_susp, p_susp, o_prio, p_prio, o_base, p_base;

        if (!g_thread_created[i]) continue;

        ot = &oracle_ThreadPool[i];
        pt = PORT_THREAD_ADDR(&g_ps, i);

        o_state = ot->state;
        p_state = gc_load_u16(pt + PORT_THREAD_STATE);
        o_susp = ot->suspend;
        p_susp = gc_load_s32(pt + PORT_THREAD_SUSPEND);
        o_prio = ot->priority;
        p_prio = gc_load_s32(pt + PORT_THREAD_PRIORITY);
        o_base = ot->base;
        p_base = gc_load_s32(pt + PORT_THREAD_BASE);

        if (o_state != p_state || o_susp != p_susp ||
            o_prio != p_prio || o_base != p_base) {
            if (mismatches < 5) {
                fprintf(stderr,
                    "  MISMATCH %s seed=%u thread[%d]: "
                    "state o=%u p=%u, susp o=%d p=%d, "
                    "prio o=%d p=%d, base o=%d p=%d\n",
                    op, seed, i,
                    o_state, p_state, o_susp, p_susp,
                    o_prio, p_prio, o_base, p_base);
            }
            mismatches++;
        }
    }

    return mismatches;
}

static int check(const char *op, uint32_t seed)
{
    int m = compare_state(op, seed);
    g_total_checks++;
    if (m > 0) {
        g_total_fail++;
        return 1;
    }
    g_total_pass++;
    return 0;
}

/* ── L0: Basic lifecycle ── */

static int test_basic_lifecycle(uint32_t seed)
{
    int n_threads, i;
    int fail = 0;

    init_both();

    n_threads = (int)rand_range(3, 8);
    if (n_threads > MAX_TEST_THREADS - 1) n_threads = MAX_TEST_THREADS - 1;

    /* Create threads with random priorities */
    for (i = 1; i <= n_threads && fail == 0; i++) {
        int32_t prio = (int32_t)rand_range(0, 31);
        uint16_t attr = (xorshift32() % 3 == 0) ?
            ORACLE_OS_THREAD_ATTR_DETACH : 0;
        create_both(i, prio, attr);
        fail += check("L0-Create", seed);
    }

    /* Resume each thread; highest priority should take over */
    for (i = 1; i <= n_threads && fail == 0; i++) {
        int32_t rc_o, rc_p;

        rc_o = oracle_OSResumeThread(&oracle_ThreadPool[i]);
        rc_p = port_OSResumeThread(&g_ps, PORT_THREAD_ADDR(&g_ps, i));

        if (rc_o != rc_p) {
            fprintf(stderr,
                "  FAIL L0-Resume seed=%u slot=%d: rc oracle=%d port=%d\n",
                seed, i, rc_o, rc_p);
            g_total_fail++;
            g_total_checks++;
            return 1;
        }
        fail += check("L0-Resume", seed);
    }

    if (opt_verbose && fail == 0) {
        printf("  L0-Lifecycle: %d threads, cur=%d OK\n",
               n_threads,
               oracle_CurrentThread ? oracle_CurrentThread->id : -1);
    }

    return fail;
}

/* ── L1: Suspend/Resume cycles ── */

static int test_suspend_resume(uint32_t seed)
{
    int n_threads, i, op;
    int fail = 0;
    int n_ops;

    init_both();

    /* Create and resume threads */
    n_threads = (int)rand_range(4, 10);
    if (n_threads > MAX_TEST_THREADS - 1) n_threads = MAX_TEST_THREADS - 1;

    for (i = 1; i <= n_threads; i++) {
        int32_t prio = (int32_t)rand_range(0, 31);
        create_both(i, prio, 1); /* all detached */
    }
    for (i = 1; i <= n_threads && fail == 0; i++) {
        oracle_OSResumeThread(&oracle_ThreadPool[i]);
        port_OSResumeThread(&g_ps, PORT_THREAD_ADDR(&g_ps, i));
        fail += check("L1-InitResume", seed);
    }

    /* Random suspend/resume ops */
    n_ops = (int)rand_range(20, 60);
    for (op = 0; op < n_ops && fail == 0; op++) {
        int slot = (int)rand_range(0, (uint32_t)n_threads);
        oracle_OSThread *ot = &oracle_ThreadPool[slot];

        if (ot->state == 0) continue; /* dead */

        if (xorshift32() % 2 == 0) {
            /* Suspend */
            int32_t rc_o, rc_p;
            rc_o = oracle_OSSuspendThread(ot);
            rc_p = port_OSSuspendThread(&g_ps, PORT_THREAD_ADDR(&g_ps, slot));
            if (rc_o != rc_p) {
                fprintf(stderr,
                    "  FAIL L1-Suspend seed=%u slot=%d: rc o=%d p=%d\n",
                    seed, slot, rc_o, rc_p);
                g_total_fail++;
                g_total_checks++;
                return 1;
            }
            fail += check("L1-Suspend", seed);
        } else {
            /* Resume */
            int32_t rc_o, rc_p;
            rc_o = oracle_OSResumeThread(ot);
            rc_p = port_OSResumeThread(&g_ps, PORT_THREAD_ADDR(&g_ps, slot));
            if (rc_o != rc_p) {
                fprintf(stderr,
                    "  FAIL L1-Resume seed=%u slot=%d: rc o=%d p=%d\n",
                    seed, slot, rc_o, rc_p);
                g_total_fail++;
                g_total_checks++;
                return 1;
            }
            fail += check("L1-Resume", seed);
        }
    }

    if (opt_verbose && fail == 0) {
        printf("  L1-SuspRes: %d threads, %d ops OK\n", n_threads, n_ops);
    }

    return fail;
}

/* ── L2: Sleep/Wakeup ── */

static int test_sleep_wakeup(uint32_t seed)
{
    int n_threads, i, op;
    int fail = 0;
    int n_ops;

    init_both();

    /* Create and resume threads */
    n_threads = (int)rand_range(4, 10);
    if (n_threads > MAX_TEST_THREADS - 1) n_threads = MAX_TEST_THREADS - 1;

    for (i = 1; i <= n_threads; i++) {
        int32_t prio = (int32_t)rand_range(0, 31);
        create_both(i, prio, 1); /* detached */
    }
    for (i = 1; i <= n_threads && fail == 0; i++) {
        oracle_OSResumeThread(&oracle_ThreadPool[i]);
        port_OSResumeThread(&g_ps, PORT_THREAD_ADDR(&g_ps, i));
        fail += check("L2-InitResume", seed);
    }

    /* Random sleep/wakeup/yield ops */
    n_ops = (int)rand_range(20, 60);
    for (op = 0; op < n_ops && fail == 0; op++) {
        uint32_t r = xorshift32() % 100;

        if (r < 35 && oracle_CurrentThread != NULL &&
            oracle_CurrentThread->state == ORACLE_OS_THREAD_STATE_RUNNING) {
            /* Sleep current thread on random wait queue */
            int wq = (int)(xorshift32() % MAX_TEST_WAITQ);

            /* Only sleep if another thread can run and scheduler enabled */
            if (oracle_RunQueueBits != 0 && oracle_Reschedule == 0) {
                oracle_OSSleepThread(&oracle_WaitQueues[wq]);
                port_OSSleepThread(&g_ps, PORT_WAITQ_ADDR(&g_ps, wq));
                fail += check("L2-Sleep", seed);
            }
        } else if (r < 70) {
            /* Wakeup a random wait queue */
            int wq = (int)(xorshift32() % MAX_TEST_WAITQ);
            oracle_OSWakeupThread(&oracle_WaitQueues[wq]);
            port_OSWakeupThread(&g_ps, PORT_WAITQ_ADDR(&g_ps, wq));
            fail += check("L2-Wakeup", seed);
        } else if (oracle_CurrentThread != NULL &&
                   oracle_CurrentThread->state == ORACLE_OS_THREAD_STATE_RUNNING) {
            /* Yield */
            oracle_OSYieldThread();
            port_OSYieldThread(&g_ps);
            fail += check("L2-Yield", seed);
        }
    }

    if (opt_verbose && fail == 0) {
        printf("  L2-SleepWake: %d threads, %d ops OK\n", n_threads, n_ops);
    }

    return fail;
}

/* ── L3: Random integration ── */

static int test_random_mix(uint32_t seed)
{
    int n_threads, i, op;
    int fail = 0;
    int n_ops;

    init_both();

    /* Create and resume a set of threads */
    n_threads = (int)rand_range(5, 12);
    if (n_threads > MAX_TEST_THREADS - 1) n_threads = MAX_TEST_THREADS - 1;

    for (i = 1; i <= n_threads; i++) {
        int32_t prio = (int32_t)rand_range(0, 31);
        uint16_t attr = (xorshift32() % 3 == 0) ?
            ORACLE_OS_THREAD_ATTR_DETACH : 0;
        create_both(i, prio, attr);
    }
    for (i = 1; i <= n_threads && fail == 0; i++) {
        oracle_OSResumeThread(&oracle_ThreadPool[i]);
        port_OSResumeThread(&g_ps, PORT_THREAD_ADDR(&g_ps, i));
        fail += check("L3-InitResume", seed);
    }

    /* Random operations */
    n_ops = (int)rand_range(30, 100);
    for (op = 0; op < n_ops && fail == 0; op++) {
        uint32_t r = xorshift32() % 100;

        if (r < 20) {
            /* Resume a random thread */
            int slot = (int)rand_range(0, (uint32_t)n_threads);
            oracle_OSThread *ot = &oracle_ThreadPool[slot];
            if (ot->state != 0 && ot->state != ORACLE_OS_THREAD_STATE_MORIBUND) {
                int32_t rc_o, rc_p;
                rc_o = oracle_OSResumeThread(ot);
                rc_p = port_OSResumeThread(&g_ps,
                    PORT_THREAD_ADDR(&g_ps, slot));
                if (rc_o != rc_p) {
                    fprintf(stderr,
                        "  FAIL L3-Resume seed=%u slot=%d: rc o=%d p=%d\n",
                        seed, slot, rc_o, rc_p);
                    g_total_fail++;
                    g_total_checks++;
                    return 1;
                }
                fail += check("L3-Resume", seed);
            }
        } else if (r < 40) {
            /* Suspend a random thread */
            int slot = (int)rand_range(0, (uint32_t)n_threads);
            oracle_OSThread *ot = &oracle_ThreadPool[slot];
            if (ot->state != 0 && ot->state != ORACLE_OS_THREAD_STATE_MORIBUND) {
                int32_t rc_o, rc_p;
                rc_o = oracle_OSSuspendThread(ot);
                rc_p = port_OSSuspendThread(&g_ps,
                    PORT_THREAD_ADDR(&g_ps, slot));
                if (rc_o != rc_p) {
                    fprintf(stderr,
                        "  FAIL L3-Suspend seed=%u slot=%d: rc o=%d p=%d\n",
                        seed, slot, rc_o, rc_p);
                    g_total_fail++;
                    g_total_checks++;
                    return 1;
                }
                fail += check("L3-Suspend", seed);
            }
        } else if (r < 55 && oracle_CurrentThread != NULL &&
                   oracle_CurrentThread->state == ORACLE_OS_THREAD_STATE_RUNNING &&
                   oracle_RunQueueBits != 0 &&
                   oracle_Reschedule == 0) {
            /* Sleep requires currentThread to genuinely be RUNNING.
             * If scheduler was disabled when the current thread was
             * suspended, oracle_CurrentThread becomes stale (READY on
             * a RunQueue).  Sleeping a stale thread would put it on
             * BOTH a RunQueue and a wait queue, corrupting both. */
            int wq = (int)(xorshift32() % MAX_TEST_WAITQ);
            oracle_OSSleepThread(&oracle_WaitQueues[wq]);
            port_OSSleepThread(&g_ps, PORT_WAITQ_ADDR(&g_ps, wq));
            fail += check("L3-Sleep", seed);
        } else if (r < 70) {
            /* Wakeup random wait queue */
            int wq = (int)(xorshift32() % MAX_TEST_WAITQ);
            oracle_OSWakeupThread(&oracle_WaitQueues[wq]);
            port_OSWakeupThread(&g_ps, PORT_WAITQ_ADDR(&g_ps, wq));
            fail += check("L3-Wakeup", seed);
        } else if (r < 80 && oracle_CurrentThread != NULL &&
                   oracle_CurrentThread->state == ORACLE_OS_THREAD_STATE_RUNNING) {
            /* Yield (only when currentThread is actually RUNNING) */
            oracle_OSYieldThread();
            port_OSYieldThread(&g_ps);
            fail += check("L3-Yield", seed);
        } else if (r < 87) {
            /* Cancel a random non-current thread */
            int slot = (int)rand_range(1, (uint32_t)n_threads);
            oracle_OSThread *ot = &oracle_ThreadPool[slot];
            if (ot->state != 0 && ot->state != ORACLE_OS_THREAD_STATE_MORIBUND &&
                ot != oracle_CurrentThread) {
                oracle_OSCancelThread(ot);
                port_OSCancelThread(&g_ps, PORT_THREAD_ADDR(&g_ps, slot));
                fail += check("L3-Cancel", seed);
            }
        } else if (r < 93 && oracle_CurrentThread != NULL &&
                   oracle_CurrentThread->state == ORACLE_OS_THREAD_STATE_RUNNING &&
                   oracle_CurrentThread->id != 0 &&
                   oracle_RunQueueBits != 0) {
            /* ExitThread: current thread exits (not default thread 0,
             * and only if another thread can take over) */
            uint32_t val = xorshift32();
            int exit_id = oracle_CurrentThread->id;
            oracle_OSExitThread(val);
            port_OSExitThread(&g_ps, val);
            fail += check("L3-Exit", seed);
            /* Mark slot as no longer usable for new operations */
            (void)exit_id;
        } else {
            /* Disable/Enable scheduler toggle */
            if (oracle_Reschedule == 0) {
                oracle_OSDisableScheduler();
                port_OSDisableScheduler(&g_ps);
                fail += check("L3-DisableSched", seed);
            } else {
                oracle_OSEnableScheduler();
                port_OSEnableScheduler(&g_ps);
                fail += check("L3-EnableSched", seed);
            }
        }
    }

    if (opt_verbose && fail == 0) {
        printf("  L3-Random: %d threads, %d ops, cur=%d OK\n",
               n_threads, n_ops,
               oracle_CurrentThread ? oracle_CurrentThread->id : -1);
    }

    return fail;
}

/* ── Run one seed ── */

static int run_seed(uint32_t seed)
{
    int fail = 0;

    /* Each level gets its own sub-seed for independent reproducibility */

    /* L0: Basic lifecycle */
    if (!opt_op || strstr("LIFECYCLE", opt_op) || strstr("L0", opt_op)) {
        g_rng = seed;
        fail += test_basic_lifecycle(seed);
    }

    /* L1: Suspend/Resume */
    if (!opt_op || strstr("SUSPEND", opt_op) || strstr("L1", opt_op)) {
        g_rng = seed ^ 0x12345678u;
        if (g_rng == 0) g_rng = 1; /* xorshift can't be 0 */
        fail += test_suspend_resume(seed);
    }

    /* L2: Sleep/Wakeup */
    if (!opt_op || strstr("SLEEP", opt_op) || strstr("L2", opt_op)) {
        g_rng = seed ^ 0x9ABCDEF0u;
        if (g_rng == 0) g_rng = 1;
        fail += test_sleep_wakeup(seed);
    }

    /* L3: Random mix */
    if (!opt_op || strstr("RANDOM", opt_op) || strstr("L3", opt_op) ||
        strstr("THREAD", opt_op)) {
        g_rng = seed ^ 0x55AA55AAu;
        if (g_rng == 0) g_rng = 1;
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
                "Usage: osthread_property_test [--seed=N] [--num-runs=N] "
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

    printf("=== OSThread Property Test ===\n");

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
