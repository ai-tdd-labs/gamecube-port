/*
 * sdk_port/os/osthread.c --- Host-side port of GC SDK OSThread + OSMutex.
 *
 * Source of truth: external/mp4-decomp/src/dolphin/os/OSThread.c
 *                  external/mp4-decomp/src/dolphin/os/OSMutex.c
 *
 * Same logic as decomp, but thread/mutex structs and queues live in gc_mem
 * (big-endian).  Queue pointers are GC addresses (u32, 0 = NULL).
 */
#include <stdint.h>
#include <string.h>
#include "osthread.h"
#include "../gc_mem.h"

/* ── Big-endian helpers ── */

static inline uint16_t load_u16be(uint32_t addr)
{
    uint8_t *p = gc_mem_ptr(addr, 2);
    return (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

static inline void store_u16be(uint32_t addr, uint16_t val)
{
    uint8_t *p = gc_mem_ptr(addr, 2);
    p[0] = (uint8_t)(val >> 8);
    p[1] = (uint8_t)(val);
}

static inline uint32_t load_u32be(uint32_t addr)
{
    uint8_t *p = gc_mem_ptr(addr, 4);
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8)  | (uint32_t)p[3];
}

static inline void store_u32be(uint32_t addr, uint32_t val)
{
    uint8_t *p = gc_mem_ptr(addr, 4);
    p[0] = (uint8_t)(val >> 24);
    p[1] = (uint8_t)(val >> 16);
    p[2] = (uint8_t)(val >> 8);
    p[3] = (uint8_t)(val);
}

/* ── Thread field access ── */

static inline uint16_t thr_get16(uint32_t t, int off) { return load_u16be(t + (uint32_t)off); }
static inline void     thr_set16(uint32_t t, int off, uint16_t v) { store_u16be(t + (uint32_t)off, v); }
static inline uint32_t thr_get32(uint32_t t, int off) { return load_u32be(t + (uint32_t)off); }
static inline void     thr_set32(uint32_t t, int off, uint32_t v) { store_u32be(t + (uint32_t)off, v); }
static inline int32_t  thr_gets32(uint32_t t, int off) { return (int32_t)load_u32be(t + (uint32_t)off); }
static inline void     thr_sets32(uint32_t t, int off, int32_t v) { store_u32be(t + (uint32_t)off, (uint32_t)v); }

/* ── Queue access ── */

static inline uint32_t q_head(uint32_t q) { return load_u32be(q + PORT_QUEUE_HEAD); }
static inline uint32_t q_tail(uint32_t q) { return load_u32be(q + PORT_QUEUE_TAIL); }
static inline void q_set_head(uint32_t q, uint32_t v) { store_u32be(q + PORT_QUEUE_HEAD, v); }
static inline void q_set_tail(uint32_t q, uint32_t v) { store_u32be(q + PORT_QUEUE_TAIL, v); }

static void q_init(uint32_t q)
{
    q_set_head(q, 0);
    q_set_tail(q, 0);
}

/* ── Mutex field access ── */

static inline uint32_t mtx_get32(uint32_t m, int off) { return load_u32be(m + (uint32_t)off); }
static inline void     mtx_set32(uint32_t m, int off, uint32_t v) { store_u32be(m + (uint32_t)off, v); }
static inline int32_t  mtx_gets32(uint32_t m, int off) { return (int32_t)load_u32be(m + (uint32_t)off); }
static inline void     mtx_sets32(uint32_t m, int off, int32_t v) { store_u32be(m + (uint32_t)off, (uint32_t)v); }

/* ────────────────────────────────────────────────────────────────────
 * Thread queue operations (parameterized by link field offsets)
 *
 * next_off / prev_off select which link to use:
 *   link:       PORT_THREAD_LINK_NEXT / PORT_THREAD_LINK_PREV
 *   linkActive: PORT_THREAD_ACTIVE_NEXT / PORT_THREAD_ACTIVE_PREV
 * ──────────────────────────────────────────────────────────────────── */

static void port_AddTail(uint32_t queue, uint32_t thread,
                         int next_off, int prev_off)
{
    uint32_t prev = q_tail(queue);
    if (prev == 0)
        q_set_head(queue, thread);
    else
        thr_set32(prev, next_off, thread);
    thr_set32(thread, prev_off, prev);
    thr_set32(thread, next_off, 0);
    q_set_tail(queue, thread);
}

static void port_AddPrio(uint32_t queue, uint32_t thread,
                         int next_off, int prev_off)
{
    uint32_t next, prev;
    int32_t thr_prio = thr_gets32(thread, PORT_THREAD_PRIORITY);

    for (next = q_head(queue);
         next && thr_gets32(next, PORT_THREAD_PRIORITY) <= thr_prio;
         next = thr_get32(next, next_off))
        ;

    if (next == 0) {
        port_AddTail(queue, thread, next_off, prev_off);
    } else {
        thr_set32(thread, next_off, next);
        prev = thr_get32(next, prev_off);
        thr_set32(next, prev_off, thread);
        thr_set32(thread, prev_off, prev);
        if (prev == 0)
            q_set_head(queue, thread);
        else
            thr_set32(prev, next_off, thread);
    }
}

static void port_RemoveItem(uint32_t queue, uint32_t thread,
                            int next_off, int prev_off)
{
    uint32_t next = thr_get32(thread, next_off);
    uint32_t prev = thr_get32(thread, prev_off);
    if (next == 0)
        q_set_tail(queue, prev);
    else
        thr_set32(next, prev_off, prev);
    if (prev == 0)
        q_set_head(queue, next);
    else
        thr_set32(prev, next_off, next);
}

static uint32_t port_RemoveHead(uint32_t queue,
                                int next_off, int prev_off)
{
    uint32_t thread = q_head(queue);
    uint32_t nh = thr_get32(thread, next_off);
    if (nh == 0)
        q_set_tail(queue, 0);
    else
        thr_set32(nh, prev_off, 0);
    q_set_head(queue, nh);
    return thread;
}

/* ── Shorthand for link / linkActive ── */

#define LINK_NEXT PORT_THREAD_LINK_NEXT
#define LINK_PREV PORT_THREAD_LINK_PREV
#define ACT_NEXT  PORT_THREAD_ACTIVE_NEXT
#define ACT_PREV  PORT_THREAD_ACTIVE_PREV

/* ────────────────────────────────────────────────────────────────────
 * Mutex queue operations (for thread's owned-mutex list)
 *
 * The mutex linked list uses PORT_MUTEX_LINK_NEXT/PREV within mutex
 * structs, and the queue head/tail lives in the thread at
 * PORT_THREAD_MUTEX_HEAD/PORT_THREAD_MUTEX_TAIL.
 * ──────────────────────────────────────────────────────────────────── */

static void port_MutexPushTail(uint32_t thread, uint32_t mutex)
{
    uint32_t prev = thr_get32(thread, PORT_THREAD_MUTEX_TAIL);
    if (prev == 0)
        thr_set32(thread, PORT_THREAD_MUTEX_HEAD, mutex);
    else
        mtx_set32(prev, PORT_MUTEX_LINK_NEXT, mutex);
    mtx_set32(mutex, PORT_MUTEX_LINK_PREV, prev);
    mtx_set32(mutex, PORT_MUTEX_LINK_NEXT, 0);
    thr_set32(thread, PORT_THREAD_MUTEX_TAIL, mutex);
}

static uint32_t port_MutexPopHead(uint32_t thread)
{
    uint32_t mutex = thr_get32(thread, PORT_THREAD_MUTEX_HEAD);
    uint32_t next = mtx_get32(mutex, PORT_MUTEX_LINK_NEXT);
    if (next == 0)
        thr_set32(thread, PORT_THREAD_MUTEX_TAIL, 0);
    else
        mtx_set32(next, PORT_MUTEX_LINK_PREV, 0);
    thr_set32(thread, PORT_THREAD_MUTEX_HEAD, next);
    return mutex;
}

static void port_MutexPopItem(uint32_t thread, uint32_t mutex)
{
    uint32_t next = mtx_get32(mutex, PORT_MUTEX_LINK_NEXT);
    uint32_t prev = mtx_get32(mutex, PORT_MUTEX_LINK_PREV);
    if (next == 0)
        thr_set32(thread, PORT_THREAD_MUTEX_TAIL, prev);
    else
        mtx_set32(next, PORT_MUTEX_LINK_PREV, prev);
    if (prev == 0)
        thr_set32(thread, PORT_THREAD_MUTEX_HEAD, next);
    else
        mtx_set32(prev, PORT_MUTEX_LINK_NEXT, next);
}

/* ────────────────────────────────────────────────────────────────────
 * cntlzw
 * ──────────────────────────────────────────────────────────────────── */

static uint32_t port_cntlzw(uint32_t x)
{
    if (x == 0) return 32;
#if defined(__GNUC__) || defined(__clang__)
    return (uint32_t)__builtin_clz(x);
#else
    uint32_t n = 0;
    if (!(x & 0xFFFF0000u)) { n += 16; x <<= 16; }
    if (!(x & 0xFF000000u)) { n += 8;  x <<= 8;  }
    if (!(x & 0xF0000000u)) { n += 4;  x <<= 4;  }
    if (!(x & 0xC0000000u)) { n += 2;  x <<= 2;  }
    if (!(x & 0x80000000u)) { n += 1; }
    return n;
#endif
}

/* ────────────────────────────────────────────────────────────────────
 * __OSGetEffectivePriority (with mutex support)
 *
 * Scan thread's owned mutexes.  For each, check if a higher-priority
 * thread is waiting.  Return the minimum (= highest) priority.
 * ──────────────────────────────────────────────────────────────────── */

static int32_t port_OSGetEffectivePriority(uint32_t thread)
{
    int32_t priority = thr_gets32(thread, PORT_THREAD_BASE);
    uint32_t mutex;

    for (mutex = thr_get32(thread, PORT_THREAD_MUTEX_HEAD);
         mutex;
         mutex = mtx_get32(mutex, PORT_MUTEX_LINK_NEXT))
    {
        uint32_t blocked = q_head(mutex + PORT_MUTEX_QUEUE_HEAD);
        if (blocked) {
            int32_t bp = thr_gets32(blocked, PORT_THREAD_PRIORITY);
            if (bp < priority) {
                priority = bp;
            }
        }
    }
    return priority;
}

/* ────────────────────────────────────────────────────────────────────
 * SetRun / UnsetRun
 * ──────────────────────────────────────────────────────────────────── */

static void port_SetRun(port_OSThreadState *st, uint32_t thread)
{
    int32_t prio = thr_gets32(thread, PORT_THREAD_PRIORITY);
    uint32_t rq = PORT_RUNQUEUE_ADDR(st, prio);
    thr_set32(thread, PORT_THREAD_QUEUE, rq);
    port_AddTail(rq, thread, LINK_NEXT, LINK_PREV);
    st->runQueueBits |= 1u << (PORT_OS_PRIORITY_MAX - prio);
    st->runQueueHint = 1;
}

static void port_UnsetRun(port_OSThreadState *st, uint32_t thread)
{
    uint32_t queue = thr_get32(thread, PORT_THREAD_QUEUE);
    int32_t prio = thr_gets32(thread, PORT_THREAD_PRIORITY);
    port_RemoveItem(queue, thread, LINK_NEXT, LINK_PREV);
    if (q_head(queue) == 0)
        st->runQueueBits &= ~(1u << (PORT_OS_PRIORITY_MAX - prio));
    thr_set32(thread, PORT_THREAD_QUEUE, 0);
}

/* ────────────────────────────────────────────────────────────────────
 * SetEffectivePriority (with mutex chain)
 * ──────────────────────────────────────────────────────────────────── */

static uint32_t port_SetEffectivePriority(port_OSThreadState *st,
                                          uint32_t thread, int32_t priority)
{
    uint16_t state = thr_get16(thread, PORT_THREAD_STATE);
    switch (state) {
        case PORT_OS_THREAD_STATE_READY:
            port_UnsetRun(st, thread);
            thr_sets32(thread, PORT_THREAD_PRIORITY, priority);
            port_SetRun(st, thread);
            break;
        case PORT_OS_THREAD_STATE_WAITING: {
            uint32_t q = thr_get32(thread, PORT_THREAD_QUEUE);
            port_RemoveItem(q, thread, LINK_NEXT, LINK_PREV);
            thr_sets32(thread, PORT_THREAD_PRIORITY, priority);
            port_AddPrio(q, thread, LINK_NEXT, LINK_PREV);
            /* Chain to mutex owner for priority inheritance */
            {
                uint32_t m = thr_get32(thread, PORT_THREAD_MUTEX);
                if (m) {
                    return mtx_get32(m, PORT_MUTEX_THREAD);
                }
            }
            break;
        }
        case PORT_OS_THREAD_STATE_RUNNING:
            st->runQueueHint = 1;
            thr_sets32(thread, PORT_THREAD_PRIORITY, priority);
            break;
    }
    return 0;
}

/* ────────────────────────────────────────────────────────────────────
 * UpdatePriority
 * ──────────────────────────────────────────────────────────────────── */

static void port_UpdatePriority(port_OSThreadState *st, uint32_t thread)
{
    int32_t priority;
    uint32_t t = thread;
    do {
        if (0 < thr_gets32(t, PORT_THREAD_SUSPEND)) break;
        priority = port_OSGetEffectivePriority(t);
        if (thr_gets32(t, PORT_THREAD_PRIORITY) == priority) break;
        t = port_SetEffectivePriority(st, t, priority);
    } while (t);
}

/* ────────────────────────────────────────────────────────────────────
 * __OSPromoteThread (reconstructed)
 * ──────────────────────────────────────────────────────────────────── */

static void port_OSPromoteThread(port_OSThreadState *st,
                                 uint32_t thread, int32_t priority)
{
    uint32_t t = thread;
    do {
        if (0 < thr_gets32(t, PORT_THREAD_SUSPEND)) break;
        if (thr_gets32(t, PORT_THREAD_PRIORITY) <= priority) break;
        t = port_SetEffectivePriority(st, t, priority);
    } while (t);
}

/* ────────────────────────────────────────────────────────────────────
 * SelectThread (simplified: no context save/load)
 * ──────────────────────────────────────────────────────────────────── */

static uint32_t port_SelectThread(port_OSThreadState *st, int yield)
{
    uint32_t currentThread;
    uint32_t nextThread;
    int32_t priority;
    uint32_t queue;

    if (0 < st->reschedule) {
        return 0;
    }

    currentThread = st->currentThread;

    if (currentThread) {
        uint16_t state = thr_get16(currentThread, PORT_THREAD_STATE);
        if (state == PORT_OS_THREAD_STATE_RUNNING) {
            if (!yield) {
                priority = (int32_t)port_cntlzw(st->runQueueBits);
                if (thr_gets32(currentThread, PORT_THREAD_PRIORITY) <= priority) {
                    return 0;
                }
            }
            thr_set16(currentThread, PORT_THREAD_STATE,
                      PORT_OS_THREAD_STATE_READY);
            port_SetRun(st, currentThread);
        }
    }

    st->currentThread = 0;

    if (st->runQueueBits == 0) {
        return 0;
    }

    st->runQueueHint = 0;

    priority = (int32_t)port_cntlzw(st->runQueueBits);
    queue = PORT_RUNQUEUE_ADDR(st, priority);
    nextThread = port_RemoveHead(queue, LINK_NEXT, LINK_PREV);
    if (q_head(queue) == 0) {
        st->runQueueBits &= ~(1u << (PORT_OS_PRIORITY_MAX - priority));
    }
    thr_set32(nextThread, PORT_THREAD_QUEUE, 0);
    thr_set16(nextThread, PORT_THREAD_STATE, PORT_OS_THREAD_STATE_RUNNING);
    st->currentThread = nextThread;
    return nextThread;
}

/* ── __OSReschedule ── */

static void port_OSReschedule(port_OSThreadState *st)
{
    if (!st->runQueueHint) return;
    port_SelectThread(st, 0);
}

/* ── __OSUnlockAllMutex ── */

static void port_OSUnlockAllMutex(port_OSThreadState *st, uint32_t thread)
{
    uint32_t mutex;
    uint32_t mq; /* mutex's waiting-thread queue addr */

    while (thr_get32(thread, PORT_THREAD_MUTEX_HEAD)) {
        mutex = port_MutexPopHead(thread);
        mtx_sets32(mutex, PORT_MUTEX_COUNT, 0);
        mtx_set32(mutex, PORT_MUTEX_THREAD, 0);
        mq = mutex + PORT_MUTEX_QUEUE_HEAD;
        port_OSWakeupThread(st, mq);
    }
}

/* ════════════════════════════════════════════════════════════════════
 *  Public API functions — Thread scheduler
 * ════════════════════════════════════════════════════════════════════ */

/* ── __OSThreadInit ── */
void port_OSThreadInit(port_OSThreadState *st, uint32_t gc_base)
{
    int prio;
    uint32_t def_addr;

    st->gc_base = gc_base;
    st->currentThread = 0;
    st->runQueueBits = 0;
    st->runQueueHint = 0;
    st->reschedule = 0;

    /* Zero all gc_mem for our region */
    memset(gc_mem_ptr(gc_base, PORT_TOTAL_SIZE), 0, PORT_TOTAL_SIZE);

    /* Init run queues */
    for (prio = PORT_OS_PRIORITY_MIN; prio <= PORT_OS_PRIORITY_MAX; prio++) {
        q_init(PORT_RUNQUEUE_ADDR(st, prio));
    }
    q_init(PORT_ACTIVEQ_ADDR(st));
    for (prio = 0; prio < PORT_MAX_WAIT_QUEUES; prio++) {
        q_init(PORT_WAITQ_ADDR(st, prio));
    }

    /* Initialize all thread IDs */
    for (prio = 0; prio < PORT_MAX_THREADS; prio++) {
        thr_set32(PORT_THREAD_ADDR(st, prio), PORT_THREAD_ID, (uint32_t)prio);
    }

    /* Default thread (slot 0): RUNNING, priority 16, detached */
    def_addr = PORT_THREAD_ADDR(st, 0);
    thr_set16(def_addr, PORT_THREAD_STATE, PORT_OS_THREAD_STATE_RUNNING);
    thr_set16(def_addr, PORT_THREAD_ATTR, PORT_OS_THREAD_ATTR_DETACH);
    thr_sets32(def_addr, PORT_THREAD_PRIORITY, 16);
    thr_sets32(def_addr, PORT_THREAD_BASE, 16);
    thr_sets32(def_addr, PORT_THREAD_SUSPEND, 0);
    thr_set32(def_addr, PORT_THREAD_VAL, 0xFFFFFFFFu);
    thr_set32(def_addr, PORT_THREAD_QUEUE, 0);
    thr_set32(def_addr, PORT_THREAD_MUTEX, 0);

    st->currentThread = def_addr;

    /* Add to active queue */
    port_AddTail(PORT_ACTIVEQ_ADDR(st), def_addr, ACT_NEXT, ACT_PREV);
}

/* ── OSCreateThread ── */
int port_OSCreateThread(port_OSThreadState *st, int slot,
                        int32_t priority, uint16_t attr)
{
    uint32_t t;

    if (priority < 0 || priority > 31) return 0;

    t = PORT_THREAD_ADDR(st, slot);
    thr_set16(t, PORT_THREAD_STATE, PORT_OS_THREAD_STATE_READY);
    thr_set16(t, PORT_THREAD_ATTR, attr & 1u);
    thr_sets32(t, PORT_THREAD_BASE, priority);
    thr_sets32(t, PORT_THREAD_PRIORITY, priority);
    thr_sets32(t, PORT_THREAD_SUSPEND, 1);
    thr_set32(t, PORT_THREAD_VAL, 0xFFFFFFFFu);
    thr_set32(t, PORT_THREAD_QUEUE, 0);
    thr_set32(t, PORT_THREAD_LINK_NEXT, 0);
    thr_set32(t, PORT_THREAD_LINK_PREV, 0);
    thr_set32(t, PORT_THREAD_MUTEX, 0);
    thr_set32(t, PORT_THREAD_MUTEX_HEAD, 0);
    thr_set32(t, PORT_THREAD_MUTEX_TAIL, 0);
    /* init queueJoin */
    thr_set32(t, PORT_THREAD_JOIN_HEAD, 0);
    thr_set32(t, PORT_THREAD_JOIN_TAIL, 0);

    /* Add to active queue */
    port_AddTail(PORT_ACTIVEQ_ADDR(st), t, ACT_NEXT, ACT_PREV);
    return 1;
}

/* ── OSResumeThread ── */
int32_t port_OSResumeThread(port_OSThreadState *st, uint32_t thread)
{
    int32_t suspendCount;

    suspendCount = thr_gets32(thread, PORT_THREAD_SUSPEND);
    thr_sets32(thread, PORT_THREAD_SUSPEND, suspendCount - 1);

    if (suspendCount - 1 < 0) {
        thr_sets32(thread, PORT_THREAD_SUSPEND, 0);
    }
    else if (suspendCount - 1 == 0) {
        uint16_t state = thr_get16(thread, PORT_THREAD_STATE);
        switch (state) {
            case PORT_OS_THREAD_STATE_READY: {
                int32_t prio = port_OSGetEffectivePriority(thread);
                thr_sets32(thread, PORT_THREAD_PRIORITY, prio);
                port_SetRun(st, thread);
                break;
            }
            case PORT_OS_THREAD_STATE_WAITING: {
                uint32_t q = thr_get32(thread, PORT_THREAD_QUEUE);
                int32_t prio;
                port_RemoveItem(q, thread, LINK_NEXT, LINK_PREV);
                prio = port_OSGetEffectivePriority(thread);
                thr_sets32(thread, PORT_THREAD_PRIORITY, prio);
                port_AddPrio(q, thread, LINK_NEXT, LINK_PREV);
                {
                    uint32_t m = thr_get32(thread, PORT_THREAD_MUTEX);
                    if (m) {
                        port_UpdatePriority(st, mtx_get32(m, PORT_MUTEX_THREAD));
                    }
                }
                break;
            }
        }
        port_OSReschedule(st);
    }
    return suspendCount;
}

/* ── OSSuspendThread ── */
int32_t port_OSSuspendThread(port_OSThreadState *st, uint32_t thread)
{
    int32_t suspendCount;

    suspendCount = thr_gets32(thread, PORT_THREAD_SUSPEND);
    thr_sets32(thread, PORT_THREAD_SUSPEND, suspendCount + 1);

    if (suspendCount == 0) {
        uint16_t state = thr_get16(thread, PORT_THREAD_STATE);
        switch (state) {
            case PORT_OS_THREAD_STATE_RUNNING:
                st->runQueueHint = 1;
                thr_set16(thread, PORT_THREAD_STATE,
                          PORT_OS_THREAD_STATE_READY);
                break;
            case PORT_OS_THREAD_STATE_READY:
                port_UnsetRun(st, thread);
                break;
            case PORT_OS_THREAD_STATE_WAITING: {
                uint32_t q = thr_get32(thread, PORT_THREAD_QUEUE);
                port_RemoveItem(q, thread, LINK_NEXT, LINK_PREV);
                thr_sets32(thread, PORT_THREAD_PRIORITY, 32);
                port_AddTail(q, thread, LINK_NEXT, LINK_PREV);
                {
                    uint32_t m = thr_get32(thread, PORT_THREAD_MUTEX);
                    if (m) {
                        port_UpdatePriority(st, mtx_get32(m, PORT_MUTEX_THREAD));
                    }
                }
                break;
            }
        }
        port_OSReschedule(st);
    }
    return suspendCount;
}

/* ── OSSleepThread ── */
void port_OSSleepThread(port_OSThreadState *st, uint32_t queue_addr)
{
    uint32_t ct = st->currentThread;

    thr_set16(ct, PORT_THREAD_STATE, PORT_OS_THREAD_STATE_WAITING);
    thr_set32(ct, PORT_THREAD_QUEUE, queue_addr);
    port_AddPrio(queue_addr, ct, LINK_NEXT, LINK_PREV);
    st->runQueueHint = 1;
    port_OSReschedule(st);
}

/* ── OSWakeupThread ── */
void port_OSWakeupThread(port_OSThreadState *st, uint32_t queue_addr)
{
    uint32_t thread;

    while (q_head(queue_addr)) {
        thread = port_RemoveHead(queue_addr, LINK_NEXT, LINK_PREV);
        thr_set16(thread, PORT_THREAD_STATE, PORT_OS_THREAD_STATE_READY);
        if (!(0 < thr_gets32(thread, PORT_THREAD_SUSPEND))) {
            port_SetRun(st, thread);
        }
    }
    port_OSReschedule(st);
}

/* ── OSYieldThread ── */
void port_OSYieldThread(port_OSThreadState *st)
{
    port_SelectThread(st, 1);
}

/* ── OSCancelThread ── */
void port_OSCancelThread(port_OSThreadState *st, uint32_t thread)
{
    uint16_t state = thr_get16(thread, PORT_THREAD_STATE);
    uint32_t join_q;

    switch (state) {
        case PORT_OS_THREAD_STATE_READY:
            if (!(0 < thr_gets32(thread, PORT_THREAD_SUSPEND))) {
                port_UnsetRun(st, thread);
            }
            break;
        case PORT_OS_THREAD_STATE_RUNNING:
            st->runQueueHint = 1;
            break;
        case PORT_OS_THREAD_STATE_WAITING: {
            uint32_t q = thr_get32(thread, PORT_THREAD_QUEUE);
            port_RemoveItem(q, thread, LINK_NEXT, LINK_PREV);
            thr_set32(thread, PORT_THREAD_QUEUE, 0);
            if (!(0 < thr_gets32(thread, PORT_THREAD_SUSPEND))) {
                uint32_t m = thr_get32(thread, PORT_THREAD_MUTEX);
                if (m) {
                    port_UpdatePriority(st, mtx_get32(m, PORT_MUTEX_THREAD));
                }
            }
            break;
        }
        default:
            return;
    }

    if (thr_get16(thread, PORT_THREAD_ATTR) & PORT_OS_THREAD_ATTR_DETACH) {
        port_RemoveItem(PORT_ACTIVEQ_ADDR(st), thread, ACT_NEXT, ACT_PREV);
        thr_set16(thread, PORT_THREAD_STATE, 0);
    } else {
        thr_set16(thread, PORT_THREAD_STATE, PORT_OS_THREAD_STATE_MORIBUND);
    }

    port_OSUnlockAllMutex(st, thread);

    join_q = thread + PORT_THREAD_JOIN_HEAD;
    port_OSWakeupThread(st, join_q);

    port_OSReschedule(st);
}

/* ── OSExitThread ── */
void port_OSExitThread(port_OSThreadState *st, uint32_t val)
{
    uint32_t ct = st->currentThread;
    uint32_t join_q;

    if (thr_get16(ct, PORT_THREAD_ATTR) & PORT_OS_THREAD_ATTR_DETACH) {
        port_RemoveItem(PORT_ACTIVEQ_ADDR(st), ct, ACT_NEXT, ACT_PREV);
        thr_set16(ct, PORT_THREAD_STATE, 0);
    } else {
        thr_set16(ct, PORT_THREAD_STATE, PORT_OS_THREAD_STATE_MORIBUND);
        thr_set32(ct, PORT_THREAD_VAL, val);
    }

    port_OSUnlockAllMutex(st, ct);

    join_q = ct + PORT_THREAD_JOIN_HEAD;
    port_OSWakeupThread(st, join_q);

    st->runQueueHint = 1;
    port_SelectThread(st, 0);
}

/* ── OSDisableScheduler / OSEnableScheduler ── */
int32_t port_OSDisableScheduler(port_OSThreadState *st)
{
    return st->reschedule++;
}

int32_t port_OSEnableScheduler(port_OSThreadState *st)
{
    return st->reschedule--;
}

/* ════════════════════════════════════════════════════════════════════
 *  Mutex API functions
 * ════════════════════════════════════════════════════════════════════ */

/* ── OSInitMutex ── */
void port_OSInitMutex(port_OSThreadState *st, uint32_t mutex_addr)
{
    (void)st;
    q_init(mutex_addr + PORT_MUTEX_QUEUE_HEAD);
    mtx_set32(mutex_addr, PORT_MUTEX_THREAD, 0);
    mtx_sets32(mutex_addr, PORT_MUTEX_COUNT, 0);
}

/* ── OSLockMutex (single-iteration, see oracle for explanation) ── */
void port_OSLockMutex(port_OSThreadState *st, uint32_t mutex_addr)
{
    uint32_t ct = st->currentThread;
    uint32_t owner = mtx_get32(mutex_addr, PORT_MUTEX_THREAD);

    if (owner == 0) {
        /* Unlocked: acquire */
        mtx_set32(mutex_addr, PORT_MUTEX_THREAD, ct);
        mtx_sets32(mutex_addr, PORT_MUTEX_COUNT,
                   mtx_gets32(mutex_addr, PORT_MUTEX_COUNT) + 1);
        port_MutexPushTail(ct, mutex_addr);
    }
    else if (owner == ct) {
        /* Recursive lock */
        mtx_sets32(mutex_addr, PORT_MUTEX_COUNT,
                   mtx_gets32(mutex_addr, PORT_MUTEX_COUNT) + 1);
    }
    else {
        /* Contended: promote owner and sleep */
        thr_set32(ct, PORT_THREAD_MUTEX, mutex_addr);
        port_OSPromoteThread(st, owner, thr_gets32(ct, PORT_THREAD_PRIORITY));
        port_OSSleepThread(st, mutex_addr + PORT_MUTEX_QUEUE_HEAD);
    }
}

/* ── OSUnlockMutex ── */
void port_OSUnlockMutex(port_OSThreadState *st, uint32_t mutex_addr)
{
    uint32_t ct = st->currentThread;

    if (mtx_get32(mutex_addr, PORT_MUTEX_THREAD) == ct) {
        int32_t count = mtx_gets32(mutex_addr, PORT_MUTEX_COUNT) - 1;
        mtx_sets32(mutex_addr, PORT_MUTEX_COUNT, count);
        if (count == 0) {
            port_MutexPopItem(ct, mutex_addr);
            mtx_set32(mutex_addr, PORT_MUTEX_THREAD, 0);
            if (thr_gets32(ct, PORT_THREAD_PRIORITY) <
                thr_gets32(ct, PORT_THREAD_BASE)) {
                thr_sets32(ct, PORT_THREAD_PRIORITY,
                           port_OSGetEffectivePriority(ct));
            }
            port_OSWakeupThread(st, mutex_addr + PORT_MUTEX_QUEUE_HEAD);
        }
    }
}

/* ── OSTryLockMutex ── */
int port_OSTryLockMutex(port_OSThreadState *st, uint32_t mutex_addr)
{
    uint32_t ct = st->currentThread;
    uint32_t owner = mtx_get32(mutex_addr, PORT_MUTEX_THREAD);
    int locked;

    if (owner == 0) {
        mtx_set32(mutex_addr, PORT_MUTEX_THREAD, ct);
        mtx_sets32(mutex_addr, PORT_MUTEX_COUNT,
                   mtx_gets32(mutex_addr, PORT_MUTEX_COUNT) + 1);
        port_MutexPushTail(ct, mutex_addr);
        locked = 1;
    }
    else if (owner == ct) {
        mtx_sets32(mutex_addr, PORT_MUTEX_COUNT,
                   mtx_gets32(mutex_addr, PORT_MUTEX_COUNT) + 1);
        locked = 1;
    }
    else {
        locked = 0;
    }
    (void)st;
    return locked;
}

/* ── OSInitCond ── */
void port_OSInitCond(port_OSThreadState *st, uint32_t cond_addr)
{
    (void)st;
    q_init(cond_addr + PORT_COND_QUEUE_HEAD);
}

/* ── OSWaitCond (OSMutex.c:140-165) ── */
void port_OSWaitCond(port_OSThreadState *st, uint32_t cond_addr, uint32_t mutex_addr)
{
    uint32_t ct = st->currentThread;
    int32_t count;

    if (mtx_get32(mutex_addr, PORT_MUTEX_THREAD) != ct) return;

    count = mtx_gets32(mutex_addr, PORT_MUTEX_COUNT);
    mtx_sets32(mutex_addr, PORT_MUTEX_COUNT, 0);
    port_MutexPopItem(ct, mutex_addr);
    mtx_set32(mutex_addr, PORT_MUTEX_THREAD, 0);

    if (thr_gets32(ct, PORT_THREAD_PRIORITY) <
        thr_gets32(ct, PORT_THREAD_BASE)) {
        thr_sets32(ct, PORT_THREAD_PRIORITY,
                   port_OSGetEffectivePriority(ct));
    }

    st->reschedule++;
    port_OSWakeupThread(st, mutex_addr + PORT_MUTEX_QUEUE_HEAD);
    st->reschedule--;
    port_OSSleepThread(st, cond_addr + PORT_COND_QUEUE_HEAD);

    /* Set thread->mutex so ProcessPendingLocks will re-lock.
     * Store saved count in val for restoration. */
    thr_set32(ct, PORT_THREAD_MUTEX, mutex_addr);
    thr_set32(ct, PORT_THREAD_VAL, (uint32_t)count);
}

/* ── OSSignalCond ── */
void port_OSSignalCond(port_OSThreadState *st, uint32_t cond_addr)
{
    port_OSWakeupThread(st, cond_addr + PORT_COND_QUEUE_HEAD);
}

/* ── OSJoinThread (non-blocking path only) ── */
int port_OSJoinThread(port_OSThreadState *st, uint32_t thread_addr, uint32_t *val)
{
    uint16_t attr = thr_get16(thread_addr, PORT_THREAD_ATTR);
    uint16_t state = thr_get16(thread_addr, PORT_THREAD_STATE);

    (void)st;

    if (attr & PORT_OS_THREAD_ATTR_DETACH) {
        return 0;
    }

    if (state == PORT_OS_THREAD_STATE_MORIBUND) {
        if (val) *val = thr_get32(thread_addr, PORT_THREAD_VAL);
        port_RemoveItem(PORT_ACTIVEQ_ADDR(st), thread_addr, ACT_NEXT, ACT_PREV);
        thr_set16(thread_addr, PORT_THREAD_STATE, 0);
        return 1;
    }

    return 0;
}

/* ════════════════════════════════════════════════════════════════════
 *  Message Queue API functions (OSMessage.c)
 * ════════════════════════════════════════════════════════════════════ */

/* ── MessageQueue field access ── */

static inline uint32_t mq_get32(uint32_t mq, int off) { return load_u32be(mq + (uint32_t)off); }
static inline void     mq_set32(uint32_t mq, int off, uint32_t v) { store_u32be(mq + (uint32_t)off, v); }
static inline int32_t  mq_gets32(uint32_t mq, int off) { return (int32_t)load_u32be(mq + (uint32_t)off); }
static inline void     mq_sets32(uint32_t mq, int off, int32_t v) { store_u32be(mq + (uint32_t)off, (uint32_t)v); }

/* ── OSInitMessageQueue ── */
void port_OSInitMessageQueue(port_OSThreadState *st, uint32_t mq_addr, int32_t msgCount)
{
    int i;
    (void)st;
    /* Init queueSend */
    q_init(mq_addr + PORT_MSGQ_SEND_HEAD);
    /* Init queueReceive */
    q_init(mq_addr + PORT_MSGQ_RECV_HEAD);
    mq_sets32(mq_addr, PORT_MSGQ_COUNT, msgCount);
    mq_sets32(mq_addr, PORT_MSGQ_FIRST, 0);
    mq_sets32(mq_addr, PORT_MSGQ_USED, 0);
    /* Clear message slots */
    for (i = 0; i < PORT_MAX_MSGS_PER_Q; i++) {
        mq_set32(mq_addr, PORT_MSGQ_MSGS + i * 4, 0);
    }
}

/* ── OSSendMessage (FIFO) ── */
int port_OSSendMessage(port_OSThreadState *st, uint32_t mq_addr,
                       uint32_t msg, int32_t flags)
{
    int32_t msgCount = mq_gets32(mq_addr, PORT_MSGQ_COUNT);
    int32_t usedCount = mq_gets32(mq_addr, PORT_MSGQ_USED);

    if (msgCount <= usedCount) {
        if (!(flags & 1)) { /* OS_MESSAGE_NOBLOCK */
            return 0;
        }
        /* BLOCK: sleep on queueSend */
        port_OSSleepThread(st, mq_addr + PORT_MSGQ_SEND_HEAD);
        return 0; /* Deferred */
    }

    {
        int32_t firstIndex = mq_gets32(mq_addr, PORT_MSGQ_FIRST);
        int32_t lastIndex = (firstIndex + usedCount) % msgCount;
        mq_set32(mq_addr, PORT_MSGQ_MSGS + lastIndex * 4, msg);
        mq_sets32(mq_addr, PORT_MSGQ_USED, usedCount + 1);
    }

    port_OSWakeupThread(st, mq_addr + PORT_MSGQ_RECV_HEAD);
    return 1;
}

/* ── OSReceiveMessage ── */
int port_OSReceiveMessage(port_OSThreadState *st, uint32_t mq_addr,
                          uint32_t *msg, int32_t flags)
{
    int32_t usedCount = mq_gets32(mq_addr, PORT_MSGQ_USED);

    if (usedCount == 0) {
        if (!(flags & 1)) { /* OS_MESSAGE_NOBLOCK */
            return 0;
        }
        /* BLOCK: sleep on queueReceive */
        port_OSSleepThread(st, mq_addr + PORT_MSGQ_RECV_HEAD);
        return 0; /* Deferred */
    }

    {
        int32_t firstIndex = mq_gets32(mq_addr, PORT_MSGQ_FIRST);
        int32_t msgCount = mq_gets32(mq_addr, PORT_MSGQ_COUNT);
        if (msg != NULL) {
            *msg = mq_get32(mq_addr, PORT_MSGQ_MSGS + firstIndex * 4);
        }
        mq_sets32(mq_addr, PORT_MSGQ_FIRST, (firstIndex + 1) % msgCount);
        mq_sets32(mq_addr, PORT_MSGQ_USED, usedCount - 1);
    }

    port_OSWakeupThread(st, mq_addr + PORT_MSGQ_SEND_HEAD);
    return 1;
}

/* ── OSJamMessage (LIFO) ── */
int port_OSJamMessage(port_OSThreadState *st, uint32_t mq_addr,
                      uint32_t msg, int32_t flags)
{
    int32_t msgCount = mq_gets32(mq_addr, PORT_MSGQ_COUNT);
    int32_t usedCount = mq_gets32(mq_addr, PORT_MSGQ_USED);

    if (msgCount <= usedCount) {
        if (!(flags & 1)) {
            return 0;
        }
        port_OSSleepThread(st, mq_addr + PORT_MSGQ_SEND_HEAD);
        return 0;
    }

    {
        int32_t firstIndex = mq_gets32(mq_addr, PORT_MSGQ_FIRST);
        firstIndex = (firstIndex + msgCount - 1) % msgCount;
        mq_set32(mq_addr, PORT_MSGQ_MSGS + firstIndex * 4, msg);
        mq_sets32(mq_addr, PORT_MSGQ_FIRST, firstIndex);
        mq_sets32(mq_addr, PORT_MSGQ_USED, usedCount + 1);
    }

    port_OSWakeupThread(st, mq_addr + PORT_MSGQ_RECV_HEAD);
    return 1;
}

/* ── ProcessPendingLocks ── */
void port_ProcessPendingLocks(port_OSThreadState *st)
{
    uint32_t ct;
    uint32_t m;

    while (1) {
        ct = st->currentThread;
        if (!ct) break;
        m = thr_get32(ct, PORT_THREAD_MUTEX);
        if (!m) break;
        if (thr_get16(ct, PORT_THREAD_STATE) != PORT_OS_THREAD_STATE_RUNNING)
            break;

        {
            uint32_t saved_count = thr_get32(ct, PORT_THREAD_VAL);
            thr_set32(ct, PORT_THREAD_MUTEX, 0);
            port_OSLockMutex(st, m);
            /* If lock was acquired and we had a saved count from WaitCond,
             * restore it. */
            if (saved_count > 0 &&
                mtx_get32(m, PORT_MUTEX_THREAD) == ct &&
                mtx_gets32(m, PORT_MUTEX_COUNT) == 1) {
                mtx_sets32(m, PORT_MUTEX_COUNT, (int32_t)saved_count);
                thr_set32(ct, PORT_THREAD_VAL, 0xFFFFFFFFu);
            }
        }
    }
}
