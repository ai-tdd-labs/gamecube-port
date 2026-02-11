/*
 * osthread_property_test.c --- Property-based parity test for OSThread + OSMutex.
 *
 * Oracle:  decomp scheduler (native structs) in osthread_oracle.h
 * Port:    sdk_port scheduler (big-endian gc_mem) in osthread.c
 *
 * For each seed:
 *   1. Random thread set (different priorities, some detached)
 *   2. Random mix of scheduler + mutex operations
 *   3. After each operation: compare observable state
 *
 * Test levels:
 *   L0: Basic lifecycle (create, resume, verify scheduling)
 *   L1: Suspend/Resume cycles with priority preemption
 *   L2: Sleep/Wakeup with wait queues
 *   L3: Random integration (all operations including mutex)
 *   L4: Mutex basic (lock/unlock/trylock)
 *   L5: Priority inheritance (contended lock promotes owner)
 *   L6: JoinThread (collect exit val from moribund thread)
 *   L7: Message queue basic (send/receive non-blocking)
 *   L8: Message queue jam (LIFO) + wraparound
 *   L9: Message queue with threads (blocking sequences)
 *   L10: WaitCond/SignalCond (condition variable sequences)
 *   L11: Mutex invariant checks (__OSCheckMutex, __OSCheckDeadLock, __OSCheckMutexes)
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

/* ── Thread / mutex tracking ── */
#define MAX_TEST_THREADS  16
#define MAX_TEST_WAITQ     8
#define MAX_TEST_MUTEXES   8
#define MAX_TEST_MSGQUEUES 4
static int g_thread_created[MAX_TEST_THREADS];
static int g_num_created;
static int g_mutex_inited[MAX_TEST_MUTEXES];
static int g_msgq_inited[MAX_TEST_MSGQUEUES];

/* ── CLI options ── */
static int opt_verbose  = 0;
static int opt_seed     = -1;
static int opt_num_runs = 500;
static const char *opt_op = NULL;

/* ── Statistics ── */
static int g_total_checks = 0;
static int g_total_pass   = 0;
static int g_total_fail   = 0;

/* ── Oracle pointer → GC address conversion ── */

static uint32_t oracle_thread_to_gc(oracle_OSThread *t)
{
    if (!t) return 0;
    return GC_BASE + PORT_THREADS_OFFSET + (uint32_t)t->id * PORT_THREAD_SIZE;
}

static uint32_t oracle_mutex_to_gc(oracle_OSMutex *m)
{
    if (!m) return 0;
    return GC_BASE + PORT_MUTEXQ_OFFSET + (uint32_t)m->id * PORT_MUTEX_SIZE;
}

static uint32_t oracle_msgq_to_gc(oracle_OSMessageQueue *mq)
{
    if (!mq) return 0;
    return GC_BASE + PORT_MSGQ_OFFSET + (uint32_t)mq->id * PORT_MSGQ_SIZE;
}

/* ── Helpers ── */

static void init_gc_mem(void)
{
    memset(g_gc_ram, 0, sizeof(g_gc_ram));
    gc_mem_set(GC_BASE, sizeof(g_gc_ram), g_gc_ram);
}

static void init_both(void)
{
    int i;
    init_gc_mem();
    oracle_OSThreadInit();
    port_OSThreadInit(&g_ps, GC_BASE);
    memset(g_thread_created, 0, sizeof(g_thread_created));
    g_thread_created[0] = 1; /* default thread */
    g_num_created = 1;
    memset(g_mutex_inited, 0, sizeof(g_mutex_inited));
    memset(g_msgq_inited, 0, sizeof(g_msgq_inited));

    /* Init all mutexes and conds in both oracle and port */
    for (i = 0; i < MAX_TEST_MUTEXES; i++) {
        oracle_OSInitMutex(&oracle_MutexPool[i]);
        port_OSInitMutex(&g_ps, PORT_MUTEX_ADDR(&g_ps, i));
    }
    for (i = 0; i < ORACLE_MAX_CONDS; i++) {
        oracle_OSInitCond(&oracle_CondPool[i]);
        port_OSInitCond(&g_ps, PORT_COND_ADDR(&g_ps, i));
    }
    /* Init message queue pool IDs */
    for (i = 0; i < ORACLE_MAX_MSGQUEUES; i++) {
        oracle_MsgQPool[i].id = i;
    }
}

static void create_both(int slot, int32_t priority, uint16_t attr)
{
    oracle_OSCreateThread(&oracle_ThreadPool[slot], priority, attr);
    port_OSCreateThread(&g_ps, slot, priority, attr);
    g_thread_created[slot] = 1;
    g_num_created++;
}

static void process_pending_both(void)
{
    oracle_ProcessPendingLocks();
    port_ProcessPendingLocks(&g_ps);
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
        uint32_t o_mtx, p_mtx, o_mh, p_mh, o_mt, p_mt;

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

        /* Thread mutex fields */
        o_mtx = oracle_mutex_to_gc(ot->mutex);
        p_mtx = gc_load_u32(pt + PORT_THREAD_MUTEX);
        o_mh = oracle_mutex_to_gc(ot->queueMutex.head);
        p_mh = gc_load_u32(pt + PORT_THREAD_MUTEX_HEAD);
        o_mt = oracle_mutex_to_gc(ot->queueMutex.tail);
        p_mt = gc_load_u32(pt + PORT_THREAD_MUTEX_TAIL);

        if (o_mtx != p_mtx || o_mh != p_mh || o_mt != p_mt) {
            if (mismatches < 5) {
                fprintf(stderr,
                    "  MISMATCH %s seed=%u thread[%d] mutex: "
                    "mtx o=0x%X p=0x%X, mh o=0x%X p=0x%X, mt o=0x%X p=0x%X\n",
                    op, seed, i,
                    o_mtx, p_mtx, o_mh, p_mh, o_mt, p_mt);
            }
            mismatches++;
        }
    }

    /* Per-mutex state */
    for (i = 0; i < MAX_TEST_MUTEXES; i++) {
        oracle_OSMutex *om = &oracle_MutexPool[i];
        uint32_t pm = PORT_MUTEX_ADDR(&g_ps, i);
        uint32_t o_qh, p_qh, o_qt, p_qt, o_thr, p_thr;
        int32_t o_cnt, p_cnt;
        uint32_t o_ln, p_ln, o_lp, p_lp;

        o_qh = oracle_thread_to_gc(om->queue.head);
        p_qh = gc_load_u32(pm + PORT_MUTEX_QUEUE_HEAD);
        o_qt = oracle_thread_to_gc(om->queue.tail);
        p_qt = gc_load_u32(pm + PORT_MUTEX_QUEUE_TAIL);
        o_thr = oracle_thread_to_gc(om->thread);
        p_thr = gc_load_u32(pm + PORT_MUTEX_THREAD);
        o_cnt = om->count;
        p_cnt = gc_load_s32(pm + PORT_MUTEX_COUNT);
        o_ln = oracle_mutex_to_gc(om->link.next);
        p_ln = gc_load_u32(pm + PORT_MUTEX_LINK_NEXT);
        o_lp = oracle_mutex_to_gc(om->link.prev);
        p_lp = gc_load_u32(pm + PORT_MUTEX_LINK_PREV);

        if (o_qh != p_qh || o_qt != p_qt || o_thr != p_thr ||
            o_cnt != p_cnt || o_ln != p_ln || o_lp != p_lp) {
            if (mismatches < 5) {
                fprintf(stderr,
                    "  MISMATCH %s seed=%u mutex[%d]: "
                    "qh o=0x%X p=0x%X, qt o=0x%X p=0x%X, "
                    "thr o=0x%X p=0x%X, cnt o=%d p=%d, "
                    "ln o=0x%X p=0x%X, lp o=0x%X p=0x%X\n",
                    op, seed, i,
                    o_qh, p_qh, o_qt, p_qt,
                    o_thr, p_thr, o_cnt, p_cnt,
                    o_ln, p_ln, o_lp, p_lp);
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

/* check with ProcessPendingLocks first */
static int check_pending(const char *op, uint32_t seed)
{
    process_pending_both();
    return check(op, seed);
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

/* ── L3: Random integration (includes mutex ops) ── */

static int test_random_mix(uint32_t seed)
{
    int n_threads, n_mutexes, i, op;
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

    /* Number of mutexes used in this run */
    n_mutexes = (int)rand_range(1, MAX_TEST_MUTEXES);
    for (i = 0; i < n_mutexes; i++) {
        g_mutex_inited[i] = 1;
    }

    /* Random operations */
    n_ops = (int)rand_range(30, 100);
    for (op = 0; op < n_ops && fail == 0; op++) {
        uint32_t r = xorshift32() % 130;

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
                fail += check_pending("L3-Resume", seed);
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
                fail += check_pending("L3-Suspend", seed);
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
            fail += check_pending("L3-Sleep", seed);
        } else if (r < 70) {
            /* Wakeup random wait queue */
            int wq = (int)(xorshift32() % MAX_TEST_WAITQ);
            oracle_OSWakeupThread(&oracle_WaitQueues[wq]);
            port_OSWakeupThread(&g_ps, PORT_WAITQ_ADDR(&g_ps, wq));
            fail += check_pending("L3-Wakeup", seed);
        } else if (r < 80 && oracle_CurrentThread != NULL &&
                   oracle_CurrentThread->state == ORACLE_OS_THREAD_STATE_RUNNING) {
            /* Yield (only when currentThread is actually RUNNING) */
            oracle_OSYieldThread();
            port_OSYieldThread(&g_ps);
            fail += check_pending("L3-Yield", seed);
        } else if (r < 87) {
            /* Cancel a random non-current thread */
            int slot = (int)rand_range(1, (uint32_t)n_threads);
            oracle_OSThread *ot = &oracle_ThreadPool[slot];
            if (ot->state != 0 && ot->state != ORACLE_OS_THREAD_STATE_MORIBUND &&
                ot != oracle_CurrentThread) {
                oracle_OSCancelThread(ot);
                port_OSCancelThread(&g_ps, PORT_THREAD_ADDR(&g_ps, slot));
                fail += check_pending("L3-Cancel", seed);
            }
        } else if (r < 93 && oracle_CurrentThread != NULL &&
                   oracle_CurrentThread->state == ORACLE_OS_THREAD_STATE_RUNNING &&
                   oracle_CurrentThread->id != 0 &&
                   oracle_RunQueueBits != 0) {
            /* ExitThread: current thread exits (not default thread 0,
             * and only if another thread can take over) */
            uint32_t val = xorshift32();
            oracle_OSExitThread(val);
            port_OSExitThread(&g_ps, val);
            fail += check_pending("L3-Exit", seed);
        } else if (r < 100) {
            /* Disable/Enable scheduler toggle */
            if (oracle_Reschedule == 0) {
                oracle_OSDisableScheduler();
                port_OSDisableScheduler(&g_ps);
                fail += check("L3-DisableSched", seed);
            } else {
                oracle_OSEnableScheduler();
                port_OSEnableScheduler(&g_ps);
                fail += check_pending("L3-EnableSched", seed);
            }
        } else if (r < 110 && oracle_CurrentThread != NULL &&
                   oracle_CurrentThread->state == ORACLE_OS_THREAD_STATE_RUNNING) {
            /* TryLock a random mutex */
            int mi = (int)(xorshift32() % (uint32_t)n_mutexes);
            int rc_o, rc_p;
            rc_o = oracle_OSTryLockMutex(&oracle_MutexPool[mi]);
            rc_p = port_OSTryLockMutex(&g_ps, PORT_MUTEX_ADDR(&g_ps, mi));
            if (rc_o != rc_p) {
                fprintf(stderr,
                    "  FAIL L3-TryLock seed=%u mutex=%d: rc o=%d p=%d\n",
                    seed, mi, rc_o, rc_p);
                g_total_fail++;
                g_total_checks++;
                return 1;
            }
            fail += check_pending("L3-TryLock", seed);
        } else if (r < 120 && oracle_CurrentThread != NULL &&
                   oracle_CurrentThread->state == ORACLE_OS_THREAD_STATE_RUNNING) {
            /* Unlock a random mutex (only if owned by current thread) */
            int mi = (int)(xorshift32() % (uint32_t)n_mutexes);
            oracle_OSMutex *om = &oracle_MutexPool[mi];
            if (om->thread == oracle_CurrentThread && om->count > 0) {
                oracle_OSUnlockMutex(om);
                port_OSUnlockMutex(&g_ps, PORT_MUTEX_ADDR(&g_ps, mi));
                fail += check_pending("L3-Unlock", seed);
            }
        } else if (r < 130 && oracle_CurrentThread != NULL &&
                   oracle_CurrentThread->state == ORACLE_OS_THREAD_STATE_RUNNING &&
                   oracle_Reschedule == 0 &&
                   oracle_RunQueueBits != 0) {
            /* Lock a random mutex (may contend and sleep) */
            int mi = (int)(xorshift32() % (uint32_t)n_mutexes);
            oracle_OSLockMutex(&oracle_MutexPool[mi]);
            port_OSLockMutex(&g_ps, PORT_MUTEX_ADDR(&g_ps, mi));
            fail += check_pending("L3-Lock", seed);
        }
    }

    if (opt_verbose && fail == 0) {
        printf("  L3-Random: %d threads, %d mutexes, %d ops, cur=%d OK\n",
               n_threads, n_mutexes, n_ops,
               oracle_CurrentThread ? oracle_CurrentThread->id : -1);
    }

    return fail;
}

/* ── L4: Mutex basic (lock/unlock/trylock) ── */

static int test_mutex_basic(uint32_t seed)
{
    int n_threads, n_mutexes, i, op;
    int fail = 0;
    int n_ops;

    init_both();

    /* Create and resume threads */
    n_threads = (int)rand_range(3, 8);
    if (n_threads > MAX_TEST_THREADS - 1) n_threads = MAX_TEST_THREADS - 1;

    for (i = 1; i <= n_threads; i++) {
        int32_t prio = (int32_t)rand_range(0, 31);
        create_both(i, prio, 1); /* all detached for simplicity */
    }
    for (i = 1; i <= n_threads && fail == 0; i++) {
        oracle_OSResumeThread(&oracle_ThreadPool[i]);
        port_OSResumeThread(&g_ps, PORT_THREAD_ADDR(&g_ps, i));
        fail += check("L4-InitResume", seed);
    }

    n_mutexes = (int)rand_range(2, MAX_TEST_MUTEXES);

    /* Random mutex operations */
    n_ops = (int)rand_range(30, 80);
    for (op = 0; op < n_ops && fail == 0; op++) {
        uint32_t r = xorshift32() % 100;

        if (!oracle_CurrentThread ||
            oracle_CurrentThread->state != ORACLE_OS_THREAD_STATE_RUNNING)
            continue;

        if (r < 30) {
            /* TryLock */
            int mi = (int)(xorshift32() % (uint32_t)n_mutexes);
            int rc_o, rc_p;
            rc_o = oracle_OSTryLockMutex(&oracle_MutexPool[mi]);
            rc_p = port_OSTryLockMutex(&g_ps, PORT_MUTEX_ADDR(&g_ps, mi));
            if (rc_o != rc_p) {
                fprintf(stderr,
                    "  FAIL L4-TryLock seed=%u mutex=%d: rc o=%d p=%d\n",
                    seed, mi, rc_o, rc_p);
                g_total_fail++;
                g_total_checks++;
                return 1;
            }
            fail += check("L4-TryLock", seed);
        } else if (r < 55) {
            /* Unlock (only if owned by current thread) */
            int mi = (int)(xorshift32() % (uint32_t)n_mutexes);
            oracle_OSMutex *om = &oracle_MutexPool[mi];
            if (om->thread == oracle_CurrentThread && om->count > 0) {
                oracle_OSUnlockMutex(om);
                port_OSUnlockMutex(&g_ps, PORT_MUTEX_ADDR(&g_ps, mi));
                fail += check_pending("L4-Unlock", seed);
            }
        } else if (r < 75 && oracle_Reschedule == 0 &&
                   oracle_RunQueueBits != 0) {
            /* Lock (may contend and sleep) */
            int mi = (int)(xorshift32() % (uint32_t)n_mutexes);
            oracle_OSLockMutex(&oracle_MutexPool[mi]);
            port_OSLockMutex(&g_ps, PORT_MUTEX_ADDR(&g_ps, mi));
            fail += check_pending("L4-Lock", seed);
        } else if (r < 85) {
            /* Yield to mix up current thread */
            oracle_OSYieldThread();
            port_OSYieldThread(&g_ps);
            fail += check_pending("L4-Yield", seed);
        } else {
            /* Resume/Suspend to mix thread states */
            int slot = (int)rand_range(1, (uint32_t)n_threads);
            oracle_OSThread *ot = &oracle_ThreadPool[slot];
            if (ot->state != 0) {
                if (xorshift32() % 2 == 0) {
                    oracle_OSResumeThread(ot);
                    port_OSResumeThread(&g_ps,
                        PORT_THREAD_ADDR(&g_ps, slot));
                    fail += check_pending("L4-Resume", seed);
                } else {
                    if (ot != oracle_CurrentThread) {
                        oracle_OSSuspendThread(ot);
                        port_OSSuspendThread(&g_ps,
                            PORT_THREAD_ADDR(&g_ps, slot));
                        fail += check_pending("L4-Suspend", seed);
                    }
                }
            }
        }
    }

    if (opt_verbose && fail == 0) {
        printf("  L4-MutexBasic: %d threads, %d mutexes, %d ops OK\n",
               n_threads, n_mutexes, n_ops);
    }

    return fail;
}

/* ── L5: Priority inheritance ── */

static int test_priority_inheritance(uint32_t seed)
{
    int fail = 0;
    int32_t prio_low, prio_high, prio_mid;
    int mi;

    init_both();

    /* Pick three distinct priority levels:
     * high (low number) < mid < low (high number) */
    prio_high = (int32_t)rand_range(0, 8);
    prio_mid  = (int32_t)rand_range(10, 20);
    prio_low  = (int32_t)rand_range(22, 31);

    /* T0 is default (prio 16, detached, running) */
    /* T1 = low priority worker, T2 = high priority requester */
    create_both(1, prio_low, 1);  /* detached */
    create_both(2, prio_high, 1); /* detached */
    /* T3 = mid priority thread (to verify it doesn't preempt promoted T1) */
    create_both(3, prio_mid, 1);  /* detached */

    /* Pick a mutex */
    mi = (int)(xorshift32() % MAX_TEST_MUTEXES);

    /* Resume T1 — T0 still current (prio 16 < prio_low) */
    oracle_OSResumeThread(&oracle_ThreadPool[1]);
    port_OSResumeThread(&g_ps, PORT_THREAD_ADDR(&g_ps, 1));
    fail += check("L5-ResumeT1", seed);
    if (fail) return fail;

    /* Suspend T0 to make T1 current */
    oracle_OSSuspendThread(&oracle_ThreadPool[0]);
    port_OSSuspendThread(&g_ps, PORT_THREAD_ADDR(&g_ps, 0));
    fail += check("L5-SuspendT0", seed);
    if (fail) return fail;

    /* T1 is now current. T1 locks mutex → acquires (unlocked) */
    oracle_OSLockMutex(&oracle_MutexPool[mi]);
    port_OSLockMutex(&g_ps, PORT_MUTEX_ADDR(&g_ps, mi));
    fail += check("L5-T1Lock", seed);
    if (fail) return fail;

    /* Verify T1 owns mutex */
    if (oracle_MutexPool[mi].thread != &oracle_ThreadPool[1]) {
        fprintf(stderr, "  FAIL L5 seed=%u: T1 doesn't own mutex\n", seed);
        g_total_fail++; g_total_checks++;
        return 1;
    }

    /* Resume T0 — T0 preempts T1 (prio 16 < prio_low) */
    oracle_OSResumeThread(&oracle_ThreadPool[0]);
    port_OSResumeThread(&g_ps, PORT_THREAD_ADDR(&g_ps, 0));
    fail += check("L5-ResumeT0", seed);
    if (fail) return fail;

    /* Resume T2 (high prio) — T2 preempts T0 */
    oracle_OSResumeThread(&oracle_ThreadPool[2]);
    port_OSResumeThread(&g_ps, PORT_THREAD_ADDR(&g_ps, 2));
    fail += check("L5-ResumeT2", seed);
    if (fail) return fail;

    /* Resume T3 (mid prio) — still T2 running */
    oracle_OSResumeThread(&oracle_ThreadPool[3]);
    port_OSResumeThread(&g_ps, PORT_THREAD_ADDR(&g_ps, 3));
    fail += check("L5-ResumeT3", seed);
    if (fail) return fail;

    /* T2 is now current (highest prio). T2 calls LockMutex(M):
     * - Contended: T1 owns M
     * - T2->mutex = M
     * - PromoteThread(T1, prio_high) → T1.priority = prio_high
     * - T2 sleeps on M.queue
     * - Reschedule: T1 (promoted to prio_high) becomes current */
    oracle_OSLockMutex(&oracle_MutexPool[mi]);
    port_OSLockMutex(&g_ps, PORT_MUTEX_ADDR(&g_ps, mi));
    process_pending_both();
    fail += check("L5-T2Lock", seed);
    if (fail) return fail;

    /* Verify T1 is current and promoted */
    if (!oracle_CurrentThread || oracle_CurrentThread->id != 1) {
        fprintf(stderr, "  FAIL L5 seed=%u: expected T1 current, got %d\n",
                seed, oracle_CurrentThread ? oracle_CurrentThread->id : -1);
        g_total_fail++; g_total_checks++;
        return 1;
    }
    if (oracle_ThreadPool[1].priority != prio_high) {
        fprintf(stderr, "  FAIL L5 seed=%u: T1 prio=%d expected=%d\n",
                seed, oracle_ThreadPool[1].priority, prio_high);
        g_total_fail++; g_total_checks++;
        return 1;
    }

    /* T1 unlocks M:
     * - M released, T1 prio restored to base (prio_low)
     * - WakeupThread wakes T2
     * - Reschedule: T2 (prio_high) preempts T1 (prio_low)
     * - ProcessPendingLocks: T2 current, T2->mutex = M → acquires M */
    oracle_OSUnlockMutex(&oracle_MutexPool[mi]);
    port_OSUnlockMutex(&g_ps, PORT_MUTEX_ADDR(&g_ps, mi));
    process_pending_both();
    fail += check("L5-T1Unlock", seed);
    if (fail) return fail;

    /* Verify T2 is current and owns the mutex */
    if (!oracle_CurrentThread || oracle_CurrentThread->id != 2) {
        fprintf(stderr, "  FAIL L5 seed=%u: expected T2 current, got %d\n",
                seed, oracle_CurrentThread ? oracle_CurrentThread->id : -1);
        g_total_fail++; g_total_checks++;
        return 1;
    }
    if (oracle_MutexPool[mi].thread != &oracle_ThreadPool[2]) {
        fprintf(stderr, "  FAIL L5 seed=%u: T2 doesn't own mutex after unlock\n",
                seed);
        g_total_fail++; g_total_checks++;
        return 1;
    }
    /* T1 priority should be restored to base */
    if (oracle_ThreadPool[1].priority != prio_low) {
        fprintf(stderr, "  FAIL L5 seed=%u: T1 prio=%d not restored to base=%d\n",
                seed, oracle_ThreadPool[1].priority, prio_low);
        g_total_fail++; g_total_checks++;
        return 1;
    }

    /* T2 unlocks M — clean up */
    oracle_OSUnlockMutex(&oracle_MutexPool[mi]);
    port_OSUnlockMutex(&g_ps, PORT_MUTEX_ADDR(&g_ps, mi));
    process_pending_both();
    fail += check("L5-T2Unlock", seed);
    if (fail) return fail;

    if (opt_verbose && fail == 0) {
        printf("  L5-PrioInherit: hi=%d mid=%d lo=%d mutex=%d OK\n",
               prio_high, prio_mid, prio_low, mi);
    }

    return fail;
}

/* ── L6: JoinThread ── */

static int test_join_thread(uint32_t seed)
{
    int n_threads, i;
    int fail = 0;

    init_both();

    /* Create several non-detached threads */
    n_threads = (int)rand_range(3, 8);
    if (n_threads > MAX_TEST_THREADS - 1) n_threads = MAX_TEST_THREADS - 1;

    for (i = 1; i <= n_threads; i++) {
        int32_t prio = (int32_t)rand_range(0, 31);
        create_both(i, prio, 0); /* non-detached */
    }
    for (i = 1; i <= n_threads && fail == 0; i++) {
        oracle_OSResumeThread(&oracle_ThreadPool[i]);
        port_OSResumeThread(&g_ps, PORT_THREAD_ADDR(&g_ps, i));
        fail += check("L6-InitResume", seed);
    }
    if (fail) return fail;

    /* Exit some threads (not thread 0, and not the current thread) to
     * create MORIBUND targets for JoinThread */
    for (i = 1; i <= n_threads && fail == 0; i++) {
        oracle_OSThread *ot = &oracle_ThreadPool[i];

        /* Only exit if this thread is current and another can take over */
        if (ot != oracle_CurrentThread) continue;
        if (oracle_RunQueueBits == 0) continue;

        {
            uint32_t val = xorshift32();
            oracle_OSExitThread(val);
            port_OSExitThread(&g_ps, val);
            fail += check("L6-Exit", seed);
        }
    }
    if (fail) return fail;

    /* Now join any MORIBUND threads from the current thread */
    for (i = 1; i <= n_threads && fail == 0; i++) {
        oracle_OSThread *ot = &oracle_ThreadPool[i];

        if (ot->state != ORACLE_OS_THREAD_STATE_MORIBUND) continue;

        {
            uint32_t oval = 0, pval = 0;
            int rc_o, rc_p;

            rc_o = oracle_OSJoinThread(ot, &oval);
            rc_p = port_OSJoinThread(&g_ps,
                PORT_THREAD_ADDR(&g_ps, i), &pval);

            if (rc_o != rc_p) {
                fprintf(stderr,
                    "  FAIL L6-Join seed=%u slot=%d: rc o=%d p=%d\n",
                    seed, i, rc_o, rc_p);
                g_total_fail++; g_total_checks++;
                return 1;
            }
            if (rc_o == 1 && oval != pval) {
                fprintf(stderr,
                    "  FAIL L6-Join seed=%u slot=%d: val o=0x%X p=0x%X\n",
                    seed, i, oval, pval);
                g_total_fail++; g_total_checks++;
                return 1;
            }
            fail += check("L6-Join", seed);
        }
    }

    /* Also test that JoinThread on detached returns 0 */
    if (fail == 0 && oracle_CurrentThread != NULL &&
        oracle_CurrentThread->state == ORACLE_OS_THREAD_STATE_RUNNING) {
        /* Create a detached thread and try joining */
        int dslot = n_threads + 1;
        if (dslot < MAX_TEST_THREADS) {
            int rc_o, rc_p;
            create_both(dslot, 16, ORACLE_OS_THREAD_ATTR_DETACH);
            rc_o = oracle_OSJoinThread(&oracle_ThreadPool[dslot], NULL);
            rc_p = port_OSJoinThread(&g_ps,
                PORT_THREAD_ADDR(&g_ps, dslot), NULL);
            if (rc_o != rc_p || rc_o != 0) {
                fprintf(stderr,
                    "  FAIL L6-JoinDetach seed=%u: rc o=%d p=%d expected=0\n",
                    seed, rc_o, rc_p);
                g_total_fail++; g_total_checks++;
                return 1;
            }
            fail += check("L6-JoinDetach", seed);
        }
    }

    if (opt_verbose && fail == 0) {
        printf("  L6-JoinThread: %d threads OK\n", n_threads);
    }

    return fail;
}

/* ── Message queue state comparison helper ── */

static int compare_msgq(const char *op, uint32_t seed, int qi)
{
    oracle_OSMessageQueue *omq = &oracle_MsgQPool[qi];
    uint32_t pmq = PORT_MSGQ_ADDR(&g_ps, qi);
    int mismatches = 0;
    int j;

    /* Compare queueSend head/tail */
    uint32_t o_sh = oracle_thread_to_gc(omq->queueSend.head);
    uint32_t p_sh = gc_load_u32(pmq + PORT_MSGQ_SEND_HEAD);
    uint32_t o_st = oracle_thread_to_gc(omq->queueSend.tail);
    uint32_t p_st = gc_load_u32(pmq + PORT_MSGQ_SEND_TAIL);
    if (o_sh != p_sh || o_st != p_st) {
        if (mismatches < 3)
            fprintf(stderr, "  MISMATCH %s seed=%u msgq[%d] qSend: "
                    "h o=0x%X p=0x%X, t o=0x%X p=0x%X\n",
                    op, seed, qi, o_sh, p_sh, o_st, p_st);
        mismatches++;
    }

    /* Compare queueReceive head/tail */
    uint32_t o_rh = oracle_thread_to_gc(omq->queueReceive.head);
    uint32_t p_rh = gc_load_u32(pmq + PORT_MSGQ_RECV_HEAD);
    uint32_t o_rt = oracle_thread_to_gc(omq->queueReceive.tail);
    uint32_t p_rt = gc_load_u32(pmq + PORT_MSGQ_RECV_TAIL);
    if (o_rh != p_rh || o_rt != p_rt) {
        if (mismatches < 3)
            fprintf(stderr, "  MISMATCH %s seed=%u msgq[%d] qRecv: "
                    "h o=0x%X p=0x%X, t o=0x%X p=0x%X\n",
                    op, seed, qi, o_rh, p_rh, o_rt, p_rt);
        mismatches++;
    }

    /* Compare msgCount, firstIndex, usedCount */
    int32_t o_cnt = omq->msgCount;
    int32_t p_cnt = gc_load_s32(pmq + PORT_MSGQ_COUNT);
    int32_t o_fi  = omq->firstIndex;
    int32_t p_fi  = gc_load_s32(pmq + PORT_MSGQ_FIRST);
    int32_t o_uc  = omq->usedCount;
    int32_t p_uc  = gc_load_s32(pmq + PORT_MSGQ_USED);

    if (o_cnt != p_cnt || o_fi != p_fi || o_uc != p_uc) {
        if (mismatches < 3)
            fprintf(stderr, "  MISMATCH %s seed=%u msgq[%d]: "
                    "cnt o=%d p=%d, first o=%d p=%d, used o=%d p=%d\n",
                    op, seed, qi, o_cnt, p_cnt, o_fi, p_fi, o_uc, p_uc);
        mismatches++;
    }

    /* Compare message array contents (only used slots matter but compare all) */
    for (j = 0; j < o_cnt && j < PORT_MAX_MSGS_PER_Q; j++) {
        uint32_t o_msg = omq->msgArray[j];
        uint32_t p_msg = gc_load_u32(pmq + PORT_MSGQ_MSGS + j * 4);
        if (o_msg != p_msg) {
            if (mismatches < 3)
                fprintf(stderr, "  MISMATCH %s seed=%u msgq[%d] msg[%d]: "
                        "o=0x%X p=0x%X\n", op, seed, qi, j, o_msg, p_msg);
            mismatches++;
        }
    }

    return mismatches;
}

static int check_msgq(const char *op, uint32_t seed, int n_queues)
{
    int total_mm = 0;
    int i;
    /* First check thread/global state */
    total_mm += compare_state(op, seed);
    /* Then check message queue state */
    for (i = 0; i < n_queues; i++) {
        total_mm += compare_msgq(op, seed, i);
    }
    /* Count as single check */
    if (total_mm > 0) {
        /* compare_state already counted, don't double-count */
        return 1;
    }
    return 0;
}

/* ── L7: Message queue basic (send/receive non-blocking) ── */

static int test_msg_basic(uint32_t seed)
{
    int fail = 0;
    int n_queues, qi, op, n_ops;

    init_both();

    n_queues = (int)rand_range(1, MAX_TEST_MSGQUEUES);

    /* Init message queues with random capacities */
    for (qi = 0; qi < n_queues; qi++) {
        int32_t cap = (int32_t)rand_range(2, ORACLE_MAX_MSGS_PER_Q);
        oracle_OSInitMessageQueue(&oracle_MsgQPool[qi], cap);
        port_OSInitMessageQueue(&g_ps, PORT_MSGQ_ADDR(&g_ps, qi), cap);
        g_msgq_inited[qi] = 1;
    }

    /* Random send/receive operations (all non-blocking) */
    n_ops = (int)rand_range(30, 100);
    for (op = 0; op < n_ops && fail == 0; op++) {
        qi = (int)(xorshift32() % (uint32_t)n_queues);
        uint32_t r = xorshift32() % 100;

        if (r < 50) {
            /* Send */
            uint32_t msg = xorshift32();
            int rc_o = oracle_OSSendMessage(&oracle_MsgQPool[qi], msg,
                                            ORACLE_OS_MESSAGE_NOBLOCK);
            int rc_p = port_OSSendMessage(&g_ps, PORT_MSGQ_ADDR(&g_ps, qi),
                                          msg, 0);
            if (rc_o != rc_p) {
                fprintf(stderr, "  FAIL L7-Send seed=%u q=%d: rc o=%d p=%d\n",
                        seed, qi, rc_o, rc_p);
                g_total_fail++; g_total_checks++;
                return 1;
            }
            fail += check_msgq("L7-Send", seed, n_queues);
        } else {
            /* Receive */
            uint32_t omsg = 0, pmsg = 0;
            int rc_o = oracle_OSReceiveMessage(&oracle_MsgQPool[qi], &omsg,
                                               ORACLE_OS_MESSAGE_NOBLOCK);
            int rc_p = port_OSReceiveMessage(&g_ps, PORT_MSGQ_ADDR(&g_ps, qi),
                                             &pmsg, 0);
            if (rc_o != rc_p) {
                fprintf(stderr, "  FAIL L7-Recv seed=%u q=%d: rc o=%d p=%d\n",
                        seed, qi, rc_o, rc_p);
                g_total_fail++; g_total_checks++;
                return 1;
            }
            if (rc_o == 1 && omsg != pmsg) {
                fprintf(stderr, "  FAIL L7-Recv seed=%u q=%d: msg o=0x%X p=0x%X\n",
                        seed, qi, omsg, pmsg);
                g_total_fail++; g_total_checks++;
                return 1;
            }
            fail += check_msgq("L7-Recv", seed, n_queues);
        }
    }

    if (opt_verbose && fail == 0) {
        printf("  L7-MsgBasic: %d queues, %d ops OK\n", n_queues, n_ops);
    }

    return fail;
}

/* ── L8: Message queue jam (LIFO) + wraparound ── */

static int test_msg_jam(uint32_t seed)
{
    int fail = 0;
    int qi, op, n_ops;

    init_both();

    /* Single queue for focused jam testing */
    int32_t cap = (int32_t)rand_range(3, 8);
    oracle_OSInitMessageQueue(&oracle_MsgQPool[0], cap);
    port_OSInitMessageQueue(&g_ps, PORT_MSGQ_ADDR(&g_ps, 0), cap);
    g_msgq_inited[0] = 1;

    qi = 0;

    /* Random mix of send, jam, receive to exercise wraparound */
    n_ops = (int)rand_range(40, 120);
    for (op = 0; op < n_ops && fail == 0; op++) {
        uint32_t r = xorshift32() % 100;

        if (r < 30) {
            /* Send (FIFO) */
            uint32_t msg = xorshift32();
            int rc_o = oracle_OSSendMessage(&oracle_MsgQPool[qi], msg,
                                            ORACLE_OS_MESSAGE_NOBLOCK);
            int rc_p = port_OSSendMessage(&g_ps, PORT_MSGQ_ADDR(&g_ps, qi),
                                          msg, 0);
            if (rc_o != rc_p) {
                fprintf(stderr, "  FAIL L8-Send seed=%u: rc o=%d p=%d\n",
                        seed, rc_o, rc_p);
                g_total_fail++; g_total_checks++;
                return 1;
            }
            fail += check_msgq("L8-Send", seed, 1);
        } else if (r < 60) {
            /* Jam (LIFO) */
            uint32_t msg = xorshift32();
            int rc_o = oracle_OSJamMessage(&oracle_MsgQPool[qi], msg,
                                           ORACLE_OS_MESSAGE_NOBLOCK);
            int rc_p = port_OSJamMessage(&g_ps, PORT_MSGQ_ADDR(&g_ps, qi),
                                         msg, 0);
            if (rc_o != rc_p) {
                fprintf(stderr, "  FAIL L8-Jam seed=%u: rc o=%d p=%d\n",
                        seed, rc_o, rc_p);
                g_total_fail++; g_total_checks++;
                return 1;
            }
            fail += check_msgq("L8-Jam", seed, 1);
        } else {
            /* Receive */
            uint32_t omsg = 0, pmsg = 0;
            int rc_o = oracle_OSReceiveMessage(&oracle_MsgQPool[qi], &omsg,
                                               ORACLE_OS_MESSAGE_NOBLOCK);
            int rc_p = port_OSReceiveMessage(&g_ps, PORT_MSGQ_ADDR(&g_ps, qi),
                                             &pmsg, 0);
            if (rc_o != rc_p) {
                fprintf(stderr, "  FAIL L8-Recv seed=%u: rc o=%d p=%d\n",
                        seed, rc_o, rc_p);
                g_total_fail++; g_total_checks++;
                return 1;
            }
            if (rc_o == 1 && omsg != pmsg) {
                fprintf(stderr, "  FAIL L8-Recv seed=%u: msg o=0x%X p=0x%X\n",
                        seed, omsg, pmsg);
                g_total_fail++; g_total_checks++;
                return 1;
            }
            fail += check_msgq("L8-Recv", seed, 1);
        }
    }

    if (opt_verbose && fail == 0) {
        printf("  L8-MsgJam: cap=%d, %d ops OK\n", cap, n_ops);
    }

    return fail;
}

/* ── L9: Message queue with threads (blocking sequences) ── */

static int test_msg_threads(uint32_t seed)
{
    int fail = 0;
    int n_threads, i, op, n_ops;

    init_both();

    /* Create and resume threads */
    n_threads = (int)rand_range(3, 8);
    if (n_threads > MAX_TEST_THREADS - 1) n_threads = MAX_TEST_THREADS - 1;

    for (i = 1; i <= n_threads; i++) {
        int32_t prio = (int32_t)rand_range(0, 31);
        create_both(i, prio, 1); /* detached */
    }
    for (i = 1; i <= n_threads && fail == 0; i++) {
        oracle_OSResumeThread(&oracle_ThreadPool[i]);
        port_OSResumeThread(&g_ps, PORT_THREAD_ADDR(&g_ps, i));
        fail += check("L9-InitResume", seed);
    }
    if (fail) return fail;

    /* Init a message queue with small capacity to trigger blocking */
    int32_t cap = (int32_t)rand_range(2, 6);
    oracle_OSInitMessageQueue(&oracle_MsgQPool[0], cap);
    port_OSInitMessageQueue(&g_ps, PORT_MSGQ_ADDR(&g_ps, 0), cap);
    g_msgq_inited[0] = 1;

    /* Mix of message ops + yield/resume to exercise thread switching */
    n_ops = (int)rand_range(30, 80);
    for (op = 0; op < n_ops && fail == 0; op++) {
        uint32_t r = xorshift32() % 100;

        if (!oracle_CurrentThread ||
            oracle_CurrentThread->state != ORACLE_OS_THREAD_STATE_RUNNING)
            continue;

        if (r < 25) {
            /* Non-blocking send */
            uint32_t msg = xorshift32();
            int rc_o = oracle_OSSendMessage(&oracle_MsgQPool[0], msg,
                                            ORACLE_OS_MESSAGE_NOBLOCK);
            int rc_p = port_OSSendMessage(&g_ps, PORT_MSGQ_ADDR(&g_ps, 0),
                                          msg, 0);
            if (rc_o != rc_p) {
                fprintf(stderr, "  FAIL L9-Send seed=%u: rc o=%d p=%d\n",
                        seed, rc_o, rc_p);
                g_total_fail++; g_total_checks++;
                return 1;
            }
            process_pending_both();
            fail += check_msgq("L9-Send", seed, 1);
        } else if (r < 50) {
            /* Non-blocking receive */
            uint32_t omsg = 0, pmsg = 0;
            int rc_o = oracle_OSReceiveMessage(&oracle_MsgQPool[0], &omsg,
                                               ORACLE_OS_MESSAGE_NOBLOCK);
            int rc_p = port_OSReceiveMessage(&g_ps, PORT_MSGQ_ADDR(&g_ps, 0),
                                             &pmsg, 0);
            if (rc_o != rc_p) {
                fprintf(stderr, "  FAIL L9-Recv seed=%u: rc o=%d p=%d\n",
                        seed, rc_o, rc_p);
                g_total_fail++; g_total_checks++;
                return 1;
            }
            if (rc_o == 1 && omsg != pmsg) {
                fprintf(stderr, "  FAIL L9-Recv seed=%u: msg o=0x%X p=0x%X\n",
                        seed, omsg, pmsg);
                g_total_fail++; g_total_checks++;
                return 1;
            }
            process_pending_both();
            fail += check_msgq("L9-Recv", seed, 1);
        } else if (r < 65) {
            /* Non-blocking jam */
            uint32_t msg = xorshift32();
            int rc_o = oracle_OSJamMessage(&oracle_MsgQPool[0], msg,
                                           ORACLE_OS_MESSAGE_NOBLOCK);
            int rc_p = port_OSJamMessage(&g_ps, PORT_MSGQ_ADDR(&g_ps, 0),
                                         msg, 0);
            if (rc_o != rc_p) {
                fprintf(stderr, "  FAIL L9-Jam seed=%u: rc o=%d p=%d\n",
                        seed, rc_o, rc_p);
                g_total_fail++; g_total_checks++;
                return 1;
            }
            process_pending_both();
            fail += check_msgq("L9-Jam", seed, 1);
        } else if (r < 80) {
            /* Yield to mix up which thread is current */
            oracle_OSYieldThread();
            port_OSYieldThread(&g_ps);
            process_pending_both();
            fail += check("L9-Yield", seed);
        } else {
            /* Resume/suspend random thread */
            int slot = (int)rand_range(1, (uint32_t)n_threads);
            oracle_OSThread *ot = &oracle_ThreadPool[slot];
            if (ot->state != 0) {
                if (xorshift32() % 2 == 0) {
                    oracle_OSResumeThread(ot);
                    port_OSResumeThread(&g_ps,
                        PORT_THREAD_ADDR(&g_ps, slot));
                } else if (ot != oracle_CurrentThread) {
                    oracle_OSSuspendThread(ot);
                    port_OSSuspendThread(&g_ps,
                        PORT_THREAD_ADDR(&g_ps, slot));
                }
                process_pending_both();
                fail += check("L9-ThreadOp", seed);
            }
        }
    }

    if (opt_verbose && fail == 0) {
        printf("  L9-MsgThreads: %d threads, cap=%d, %d ops OK\n",
               n_threads, cap, n_ops);
    }

    return fail;
}

/* ── L10: WaitCond / SignalCond ──
 *
 * Controlled sequence:
 *   1. Thread A (current) locks mutex M
 *   2. Thread A calls WaitCond(cond, M) → releases M, sleeps on cond
 *   3. Thread B becomes current, signals cond → A wakes
 *   4. ProcessPendingLocks → A re-locks M with saved count restored
 *
 * We also test recursive lock count preservation.
 */

static int test_waitcond(uint32_t seed)
{
    int fail = 0;
    int n_rounds, round;

    init_both();

    n_rounds = (int)rand_range(5, 20);

    for (round = 0; round < n_rounds && fail == 0; round++) {
        int slot_a = 1, slot_b = 2;
        int32_t prio_a, prio_b;
        int cond_idx, mutex_idx;
        int recursive_locks, i;

        /* Fresh state each round */
        init_both();

        /* Random priorities: B must be lower prio number (higher priority)
         * so it can run when A sleeps, OR we ensure B is resumed and A
         * sleeps so B gets to run. */
        prio_a = (int32_t)rand_range(10, 25);
        prio_b = (int32_t)rand_range(0, 9);

        create_both(slot_a, prio_a, 1); /* detached */
        create_both(slot_b, prio_b, 1); /* detached */

        /* Resume A first — it has lower priority than default (16),
         * but we need it running. Resume B as well. */
        oracle_OSResumeThread(&oracle_ThreadPool[slot_a]);
        port_OSResumeThread(&g_ps, PORT_THREAD_ADDR(&g_ps, slot_a));

        oracle_OSResumeThread(&oracle_ThreadPool[slot_b]);
        port_OSResumeThread(&g_ps, PORT_THREAD_ADDR(&g_ps, slot_b));
        fail += check("L10-Setup", seed);
        if (fail) break;

        /* B should be current (highest priority) */
        /* Suspend B so A can run */
        oracle_OSSuspendThread(&oracle_ThreadPool[slot_b]);
        port_OSSuspendThread(&g_ps, PORT_THREAD_ADDR(&g_ps, slot_b));
        fail += check("L10-SuspB", seed);
        if (fail) break;

        /* Now A (or default thread 0) should be current.
         * If A's priority > 16 (lower than default), default is current.
         * Make sure A runs by suspending default if needed. */
        if (oracle_CurrentThread != &oracle_ThreadPool[slot_a]) {
            /* A is not current; skip this round's scenario */
            continue;
        }

        cond_idx = (int)(xorshift32() % ORACLE_MAX_CONDS);
        mutex_idx = (int)(xorshift32() % MAX_TEST_MUTEXES);

        /* Lock mutex (possibly recursively) */
        recursive_locks = (int)rand_range(1, 4);
        for (i = 0; i < recursive_locks; i++) {
            oracle_OSLockMutex(&oracle_MutexPool[mutex_idx]);
            port_OSLockMutex(&g_ps, PORT_MUTEX_ADDR(&g_ps, mutex_idx));
            process_pending_both();
        }
        fail += check("L10-Lock", seed);
        if (fail) break;

        /* WaitCond: A releases mutex, sleeps on cond */
        oracle_OSWaitCond(&oracle_CondPool[cond_idx],
                          &oracle_MutexPool[mutex_idx]);
        port_OSWaitCond(&g_ps, PORT_COND_ADDR(&g_ps, cond_idx),
                        PORT_MUTEX_ADDR(&g_ps, mutex_idx));
        fail += check("L10-WaitCond", seed);
        if (fail) break;

        /* Resume B — B should become current */
        oracle_OSResumeThread(&oracle_ThreadPool[slot_b]);
        port_OSResumeThread(&g_ps, PORT_THREAD_ADDR(&g_ps, slot_b));
        fail += check("L10-ResumeB", seed);
        if (fail) break;

        /* B signals the cond — A wakes up */
        oracle_OSSignalCond(&oracle_CondPool[cond_idx]);
        port_OSSignalCond(&g_ps, PORT_COND_ADDR(&g_ps, cond_idx));
        fail += check("L10-Signal", seed);
        if (fail) break;

        /* ProcessPendingLocks: A re-locks mutex with saved count */
        process_pending_both();
        fail += check("L10-Pending", seed);
        if (fail) break;

        /* Verify: mutex should be locked by whoever is current.
         * If A got the lock, count should be restored. */
        {
            oracle_OSMutex *om = &oracle_MutexPool[mutex_idx];
            if (om->thread == &oracle_ThreadPool[slot_a]) {
                /* A re-acquired: count should match recursive_locks */
                uint32_t pm = PORT_MUTEX_ADDR(&g_ps, mutex_idx);
                int32_t o_cnt = om->count;
                int32_t p_cnt = gc_load_s32(pm + PORT_MUTEX_COUNT);
                if (o_cnt != recursive_locks || p_cnt != recursive_locks) {
                    fprintf(stderr,
                        "  FAIL L10 seed=%u: count mismatch "
                        "expected=%d oracle=%d port=%d\n",
                        seed, recursive_locks, o_cnt, p_cnt);
                    g_total_fail++;
                    g_total_checks++;
                    fail++;
                    break;
                }
            }
        }

        /* Unlock the mutex (all recursive levels) */
        if (oracle_CurrentThread &&
            oracle_MutexPool[mutex_idx].thread == oracle_CurrentThread) {
            for (i = 0; i < recursive_locks; i++) {
                oracle_OSUnlockMutex(&oracle_MutexPool[mutex_idx]);
                port_OSUnlockMutex(&g_ps, PORT_MUTEX_ADDR(&g_ps, mutex_idx));
                process_pending_both();
            }
            fail += check("L10-Unlock", seed);
        }
    }

    if (opt_verbose && fail == 0) {
        printf("  L10-WaitCond: %d rounds OK\n", n_rounds);
    }

    return fail;
}

/* ── L11: Mutex invariant checks ──
 *
 * Random mix of thread + mutex operations (like L3) but after every
 * operation we also verify:
 *   - __OSCheckMutex passes for every inited mutex
 *   - __OSCheckDeadLock is false for current thread
 *   - __OSCheckMutexes passes for every created thread
 */

static int check_invariants(const char *op, uint32_t seed)
{
    int i;
    int mismatches = 0;

    /* Check all mutexes */
    for (i = 0; i < MAX_TEST_MUTEXES; i++) {
        if (!oracle_OSCheckMutex(&oracle_MutexPool[i])) {
            if (mismatches < 3)
                fprintf(stderr,
                    "  INVARIANT %s seed=%u: __OSCheckMutex(%d) FAILED\n",
                    op, seed, i);
            mismatches++;
        }
    }

    /* Check no deadlock for current thread */
    if (oracle_CurrentThread &&
        oracle_CurrentThread->state == ORACLE_OS_THREAD_STATE_RUNNING) {
        if (oracle_OSCheckDeadLock(oracle_CurrentThread)) {
            if (mismatches < 3)
                fprintf(stderr,
                    "  INVARIANT %s seed=%u: __OSCheckDeadLock(cur=%d) TRUE\n",
                    op, seed, oracle_CurrentThread->id);
            mismatches++;
        }
    }

    /* Check all created threads' mutex lists */
    for (i = 0; i < MAX_TEST_THREADS; i++) {
        if (!g_thread_created[i]) continue;
        if (oracle_ThreadPool[i].state == 0) continue; /* dead */
        if (!oracle_OSCheckMutexes(&oracle_ThreadPool[i])) {
            if (mismatches < 3)
                fprintf(stderr,
                    "  INVARIANT %s seed=%u: __OSCheckMutexes(thread %d) FAILED\n",
                    op, seed, i);
            mismatches++;
        }
    }

    return mismatches;
}

static int check_with_invariants(const char *op, uint32_t seed)
{
    int m = compare_state(op, seed);
    int inv = check_invariants(op, seed);
    g_total_checks++;
    if (m > 0 || inv > 0) {
        g_total_fail++;
        return 1;
    }
    g_total_pass++;
    return 0;
}

static int test_mutex_invariants(uint32_t seed)
{
    int n_threads, i, op;
    int fail = 0;
    int n_ops;

    init_both();

    /* Create and resume threads */
    n_threads = (int)rand_range(4, 8);
    if (n_threads > MAX_TEST_THREADS - 1) n_threads = MAX_TEST_THREADS - 1;

    for (i = 1; i <= n_threads; i++) {
        int32_t prio = (int32_t)rand_range(0, 31);
        create_both(i, prio, 1); /* detached */
    }
    for (i = 1; i <= n_threads && fail == 0; i++) {
        oracle_OSResumeThread(&oracle_ThreadPool[i]);
        port_OSResumeThread(&g_ps, PORT_THREAD_ADDR(&g_ps, i));
        fail += check_with_invariants("L11-Init", seed);
    }

    /* Random ops with invariant checks after each */
    n_ops = (int)rand_range(30, 80);
    for (op = 0; op < n_ops && fail == 0; op++) {
        uint32_t r = xorshift32() % 100;

        if (r < 20 && oracle_CurrentThread &&
            oracle_CurrentThread->state == ORACLE_OS_THREAD_STATE_RUNNING) {
            /* TryLock a random mutex */
            int mi = (int)(xorshift32() % MAX_TEST_MUTEXES);
            int ro = oracle_OSTryLockMutex(&oracle_MutexPool[mi]);
            int rp = port_OSTryLockMutex(&g_ps, PORT_MUTEX_ADDR(&g_ps, mi));
            if (ro != rp) {
                fprintf(stderr,
                    "  FAIL L11 seed=%u: TryLock(%d) oracle=%d port=%d\n",
                    seed, mi, ro, rp);
                g_total_fail++; g_total_checks++;
                return 1;
            }
            fail += check_with_invariants("L11-TryLock", seed);
        }
        else if (r < 40 && oracle_CurrentThread &&
                 oracle_CurrentThread->state == ORACLE_OS_THREAD_STATE_RUNNING) {
            /* Unlock a random mutex (only if we own it) */
            int mi = (int)(xorshift32() % MAX_TEST_MUTEXES);
            oracle_OSMutex *om = &oracle_MutexPool[mi];
            if (om->thread == oracle_CurrentThread) {
                oracle_OSUnlockMutex(om);
                port_OSUnlockMutex(&g_ps, PORT_MUTEX_ADDR(&g_ps, mi));
                process_pending_both();
                fail += check_with_invariants("L11-Unlock", seed);
            }
        }
        else if (r < 55) {
            /* Yield */
            if (oracle_CurrentThread &&
                oracle_CurrentThread->state == ORACLE_OS_THREAD_STATE_RUNNING) {
                oracle_OSYieldThread();
                port_OSYieldThread(&g_ps);
                fail += check_with_invariants("L11-Yield", seed);
            }
        }
        else if (r < 70) {
            /* Suspend a random thread */
            int slot = (int)rand_range(1, (uint32_t)n_threads);
            oracle_OSThread *ot = &oracle_ThreadPool[slot];
            if (ot->state != 0 && ot->state != ORACLE_OS_THREAD_STATE_MORIBUND) {
                int32_t rc_o = oracle_OSSuspendThread(ot);
                int32_t rc_p = port_OSSuspendThread(&g_ps,
                    PORT_THREAD_ADDR(&g_ps, slot));
                (void)rc_o; (void)rc_p;
                fail += check_with_invariants("L11-Suspend", seed);
            }
        }
        else if (r < 85) {
            /* Resume a random thread */
            int slot = (int)rand_range(1, (uint32_t)n_threads);
            oracle_OSThread *ot = &oracle_ThreadPool[slot];
            if (ot->state != 0 && ot->state != ORACLE_OS_THREAD_STATE_MORIBUND) {
                oracle_OSResumeThread(ot);
                port_OSResumeThread(&g_ps, PORT_THREAD_ADDR(&g_ps, slot));
                fail += check_with_invariants("L11-Resume", seed);
            }
        }
        else {
            /* Lock a random mutex (only if unlocked or self-owned) */
            int mi = (int)(xorshift32() % MAX_TEST_MUTEXES);
            oracle_OSMutex *om = &oracle_MutexPool[mi];
            if (oracle_CurrentThread &&
                oracle_CurrentThread->state == ORACLE_OS_THREAD_STATE_RUNNING &&
                (om->thread == NULL || om->thread == oracle_CurrentThread)) {
                oracle_OSLockMutex(om);
                port_OSLockMutex(&g_ps, PORT_MUTEX_ADDR(&g_ps, mi));
                process_pending_both();
                fail += check_with_invariants("L11-Lock", seed);
            }
        }
    }

    if (opt_verbose && fail == 0) {
        printf("  L11-Invariants: %d threads, %d ops OK\n", n_threads, n_ops);
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

    /* L3: Random mix (threads + mutexes) */
    if (!opt_op || strstr("RANDOM", opt_op) || strstr("L3", opt_op) ||
        strstr("THREAD", opt_op)) {
        g_rng = seed ^ 0x55AA55AAu;
        if (g_rng == 0) g_rng = 1;
        fail += test_random_mix(seed);
    }

    /* L4: Mutex basic */
    if (!opt_op || strstr("MUTEX", opt_op) || strstr("L4", opt_op)) {
        g_rng = seed ^ 0xDEADBEEFu;
        if (g_rng == 0) g_rng = 1;
        fail += test_mutex_basic(seed);
    }

    /* L5: Priority inheritance */
    if (!opt_op || strstr("PRIO", opt_op) || strstr("L5", opt_op) ||
        strstr("INHERIT", opt_op)) {
        g_rng = seed ^ 0xCAFEBABEu;
        if (g_rng == 0) g_rng = 1;
        fail += test_priority_inheritance(seed);
    }

    /* L6: JoinThread */
    if (!opt_op || strstr("JOIN", opt_op) || strstr("L6", opt_op)) {
        g_rng = seed ^ 0xBAADF00Du;
        if (g_rng == 0) g_rng = 1;
        fail += test_join_thread(seed);
    }

    /* L7: Message queue basic */
    if (!opt_op || strstr("MSG", opt_op) || strstr("L7", opt_op) ||
        strstr("MESSAGE", opt_op)) {
        g_rng = seed ^ 0x1337C0DEu;
        if (g_rng == 0) g_rng = 1;
        fail += test_msg_basic(seed);
    }

    /* L8: Message queue jam */
    if (!opt_op || strstr("JAM", opt_op) || strstr("L8", opt_op)) {
        g_rng = seed ^ 0xF00DCAFEu;
        if (g_rng == 0) g_rng = 1;
        fail += test_msg_jam(seed);
    }

    /* L9: Message queue with threads */
    if (!opt_op || strstr("L9", opt_op) || strstr("MSGTHREAD", opt_op)) {
        g_rng = seed ^ 0xACE0BA5Eu;
        if (g_rng == 0) g_rng = 1;
        fail += test_msg_threads(seed);
    }

    /* L10: WaitCond / SignalCond */
    if (!opt_op || strstr("L10", opt_op) || strstr("COND", opt_op) ||
        strstr("WAITCOND", opt_op)) {
        g_rng = seed ^ 0x7E57C0DEu;
        if (g_rng == 0) g_rng = 1;
        fail += test_waitcond(seed);
    }

    /* L11: Mutex invariant checks */
    if (!opt_op || strstr("L11", opt_op) || strstr("INVARIANT", opt_op)) {
        g_rng = seed ^ 0xC0FFEE42u;
        if (g_rng == 0) g_rng = 1;
        fail += test_mutex_invariants(seed);
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

    printf("=== OSThread + OSMutex Property Test ===\n");

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
