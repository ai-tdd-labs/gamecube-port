/*
 * sdk_port/os/osthread.c --- Host-side port of GC SDK OSThread scheduler.
 *
 * Source of truth: external/mp4-decomp/src/dolphin/os/OSThread.c
 *
 * Same logic as decomp, but thread structs and queues live in gc_mem
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

/* ────────────────────────────────────────────────────────────────────
 * Queue operations (parameterized by link field offsets)
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

/* RemoveHead: returns the removed thread's GC address via pointer */
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
 * __OSGetEffectivePriority (no mutexes: returns base)
 * ──────────────────────────────────────────────────────────────────── */

static int32_t port_OSGetEffectivePriority(uint32_t thread)
{
    return thr_gets32(thread, PORT_THREAD_BASE);
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
 * SetEffectivePriority / UpdatePriority
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
            break;
        }
        case PORT_OS_THREAD_STATE_RUNNING:
            st->runQueueHint = 1;
            thr_sets32(thread, PORT_THREAD_PRIORITY, priority);
            break;
    }
    return 0; /* no mutex chain */
}

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

/* ════════════════════════════════════════════════════════════════════
 *  Public API functions
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

    /* __OSUnlockAllMutex — stub */

    /* OSWakeupThread(&thread->queueJoin) */
    join_q = thread + PORT_THREAD_JOIN_HEAD;
    /* queueJoin is embedded in the thread struct at JOIN_HEAD/JOIN_TAIL.
     * We need to treat it as a queue starting at that address.
     * Since our queue functions expect PORT_QUEUE_HEAD at offset 0 and
     * PORT_QUEUE_TAIL at offset 4, and JOIN_HEAD/JOIN_TAIL are adjacent
     * (36/40), we can pass the address of JOIN_HEAD directly. */
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

    /* __OSUnlockAllMutex — stub */

    /* OSWakeupThread(&ct->queueJoin) */
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
