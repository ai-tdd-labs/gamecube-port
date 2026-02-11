/*
 * osthread_oracle.h --- Oracle for OSThread property tests.
 *
 * Exact copy of decomp OSThread scheduler logic with oracle_ prefix.
 * Hardware calls (context save/load, interrupts) are stripped/mocked.
 * Mutex fields stubbed (OSMutex chain not yet implemented):
 *   __OSGetEffectivePriority returns thread->base, UpdatePriority is no-op.
 *
 * Source of truth:
 *   external/mp4-decomp/src/dolphin/os/OSThread.c
 *   external/mp4-decomp/include/dolphin/os/OSThread.h
 */
#pragma once

#include <stdint.h>
#include <string.h>

/* ── Constants ── */
#define ORACLE_OS_THREAD_STATE_READY    1
#define ORACLE_OS_THREAD_STATE_RUNNING  2
#define ORACLE_OS_THREAD_STATE_WAITING  4
#define ORACLE_OS_THREAD_STATE_MORIBUND 8

#define ORACLE_OS_THREAD_ATTR_DETACH 0x0001u

#define ORACLE_OS_PRIORITY_MIN 0   /* highest */
#define ORACLE_OS_PRIORITY_MAX 31  /* lowest */

#define ORACLE_MAX_THREADS      32
#define ORACLE_MAX_WAIT_QUEUES  16

/* ── Types ── */
typedef struct oracle_OSThread oracle_OSThread;
typedef struct oracle_OSThreadQueue oracle_OSThreadQueue;
typedef struct oracle_OSThreadLink oracle_OSThreadLink;

struct oracle_OSThreadQueue {
    oracle_OSThread *head;
    oracle_OSThread *tail;
};

struct oracle_OSThreadLink {
    oracle_OSThread *next;
    oracle_OSThread *prev;
};

struct oracle_OSThread {
    int      id;          /* index in pool, for identification */
    uint16_t state;
    uint16_t attr;
    int32_t  suspend;
    int32_t  priority;    /* effective priority */
    int32_t  base;        /* base priority */
    uint32_t val;         /* exit value */
    oracle_OSThreadQueue *queue;     /* current queue thread is in */
    oracle_OSThreadLink  link;       /* link within queue */
    oracle_OSThreadQueue queueJoin;  /* threads waiting to join this thread */
    oracle_OSThreadLink  linkActive; /* link in active thread queue */
};

/* ── Global state ── */
static oracle_OSThread      oracle_ThreadPool[ORACLE_MAX_THREADS];
static int                  oracle_ThreadUsed[ORACLE_MAX_THREADS];
static oracle_OSThread     *oracle_CurrentThread;
static uint32_t             oracle_RunQueueBits;
static int32_t              oracle_RunQueueHint;
static int32_t              oracle_Reschedule;
static oracle_OSThreadQueue oracle_RunQueue[32];
static oracle_OSThreadQueue oracle_ActiveThreadQueue;
static oracle_OSThreadQueue oracle_WaitQueues[ORACLE_MAX_WAIT_QUEUES];

/* ────────────────────────────────────────────────────────────────────
 * Queue macros (OSThread.c:24-83, exact logic)
 *
 * Token-paste `link` field: `link` for run/wait queues,
 * `linkActive` for the active-thread queue.
 * ──────────────────────────────────────────────────────────────────── */

#define oracle_AddTail(queue, thread, link)                          \
    do {                                                             \
        oracle_OSThread *_prev;                                      \
        _prev = (queue)->tail;                                       \
        if (_prev == NULL)                                           \
            (queue)->head = (thread);                                \
        else                                                         \
            _prev->link.next = (thread);                             \
        (thread)->link.prev = _prev;                                 \
        (thread)->link.next = NULL;                                  \
        (queue)->tail = (thread);                                    \
    } while (0)

#define oracle_AddPrio(queue, thread, link)                          \
    do {                                                             \
        oracle_OSThread *_prev, *_next;                              \
        for (_next = (queue)->head;                                  \
             _next && _next->priority <= (thread)->priority;         \
             _next = _next->link.next)                               \
            ;                                                        \
        if (_next == NULL)                                           \
            oracle_AddTail(queue, thread, link);                     \
        else {                                                       \
            (thread)->link.next = _next;                             \
            _prev = _next->link.prev;                                \
            _next->link.prev = (thread);                             \
            (thread)->link.prev = _prev;                             \
            if (_prev == NULL)                                       \
                (queue)->head = (thread);                            \
            else                                                     \
                _prev->link.next = (thread);                         \
        }                                                            \
    } while (0)

#define oracle_RemoveItem(queue, thread, link)                       \
    do {                                                             \
        oracle_OSThread *_next, *_prev;                              \
        _next = (thread)->link.next;                                 \
        _prev = (thread)->link.prev;                                 \
        if (_next == NULL)                                           \
            (queue)->tail = _prev;                                   \
        else                                                         \
            _next->link.prev = _prev;                                \
        if (_prev == NULL)                                           \
            (queue)->head = _next;                                   \
        else                                                         \
            _prev->link.next = _next;                                \
    } while (0)

#define oracle_RemoveHead(queue, thread, link)                       \
    do {                                                             \
        oracle_OSThread *_nh;                                        \
        (thread) = (queue)->head;                                    \
        _nh = (thread)->link.next;                                   \
        if (_nh == NULL)                                             \
            (queue)->tail = NULL;                                    \
        else                                                         \
            _nh->link.prev = NULL;                                   \
        (queue)->head = _nh;                                         \
    } while (0)

/* ────────────────────────────────────────────────────────────────────
 * cntlzw --- count leading zeros (PPC instruction)
 * ──────────────────────────────────────────────────────────────────── */
static uint32_t oracle_cntlzw(uint32_t x)
{
    if (x == 0) return 32;
#if defined(__GNUC__) || defined(__clang__)
    return (uint32_t)__builtin_clz(x);
#else
    /* fallback */
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
 * Init / helper functions
 * ──────────────────────────────────────────────────────────────────── */

static void oracle_OSInitThreadQueue(oracle_OSThreadQueue *queue)
{
    queue->head = queue->tail = NULL;
}

/* __OSGetEffectivePriority (OSThread.c:181-195)
 * Without mutex support: returns thread->base */
static int32_t oracle_OSGetEffectivePriority(oracle_OSThread *thread)
{
    return thread->base;
}

/* ────────────────────────────────────────────────────────────────────
 * SetRun / UnsetRun (OSThread.c:162-178)
 * ──────────────────────────────────────────────────────────────────── */

static void oracle_SetRun(oracle_OSThread *thread)
{
    thread->queue = &oracle_RunQueue[thread->priority];
    oracle_AddTail(thread->queue, thread, link);
    oracle_RunQueueBits |= 1u << (ORACLE_OS_PRIORITY_MAX - thread->priority);
    oracle_RunQueueHint = 1;
}

static void oracle_UnsetRun(oracle_OSThread *thread)
{
    oracle_OSThreadQueue *queue;
    queue = thread->queue;
    oracle_RemoveItem(queue, thread, link);
    if (queue->head == NULL)
        oracle_RunQueueBits &= ~(1u << (ORACLE_OS_PRIORITY_MAX - thread->priority));
    thread->queue = NULL;
}

/* ────────────────────────────────────────────────────────────────────
 * SetEffectivePriority / UpdatePriority (OSThread.c:197-235)
 * ──────────────────────────────────────────────────────────────────── */

static oracle_OSThread *oracle_SetEffectivePriority(
    oracle_OSThread *thread, int32_t priority)
{
    switch (thread->state) {
        case ORACLE_OS_THREAD_STATE_READY:
            oracle_UnsetRun(thread);
            thread->priority = priority;
            oracle_SetRun(thread);
            break;
        case ORACLE_OS_THREAD_STATE_WAITING:
            oracle_RemoveItem(thread->queue, thread, link);
            thread->priority = priority;
            oracle_AddPrio(thread->queue, thread, link);
            /* thread->mutex->thread would go here for priority inheritance */
            break;
        case ORACLE_OS_THREAD_STATE_RUNNING:
            oracle_RunQueueHint = 1;
            thread->priority = priority;
            break;
    }
    return NULL; /* no mutex chain traversal */
}

static void oracle_UpdatePriority(oracle_OSThread *thread)
{
    int32_t priority;
    do {
        if (0 < thread->suspend) break;
        priority = oracle_OSGetEffectivePriority(thread);
        if (thread->priority == priority) break;
        thread = oracle_SetEffectivePriority(thread, priority);
    } while (thread);
}

/* ────────────────────────────────────────────────────────────────────
 * SelectThread (OSThread.c:244-304, simplified: no context save/load)
 *
 * In the real GC, SelectThread saves the current context and loads
 * the next thread's context.  Here we just update the current-thread
 * pointer and states — the test driver drives execution externally.
 * ──────────────────────────────────────────────────────────────────── */

static oracle_OSThread *oracle_SelectThread(int yield)
{
    oracle_OSThread *currentThread;
    oracle_OSThread *nextThread;
    int32_t priority;
    oracle_OSThreadQueue *queue;

    if (0 < oracle_Reschedule) {
        return NULL;
    }

    currentThread = oracle_CurrentThread;

    if (currentThread) {
        if (currentThread->state == ORACLE_OS_THREAD_STATE_RUNNING) {
            if (!yield) {
                priority = (int32_t)oracle_cntlzw(oracle_RunQueueBits);
                if (currentThread->priority <= priority) {
                    return NULL;
                }
            }
            currentThread->state = ORACLE_OS_THREAD_STATE_READY;
            oracle_SetRun(currentThread);
        }
        /* skip OSSaveContext — oracle doesn't context-switch */
    }

    oracle_CurrentThread = NULL;

    if (oracle_RunQueueBits == 0) {
        /* Idle state: nothing runnable */
        return NULL;
    }

    oracle_RunQueueHint = 0;

    priority = (int32_t)oracle_cntlzw(oracle_RunQueueBits);
    queue = &oracle_RunQueue[priority];
    oracle_RemoveHead(queue, nextThread, link);
    if (queue->head == NULL) {
        oracle_RunQueueBits &= ~(1u << (ORACLE_OS_PRIORITY_MAX - priority));
    }
    nextThread->queue = NULL;
    nextThread->state = ORACLE_OS_THREAD_STATE_RUNNING;
    oracle_CurrentThread = nextThread;
    return nextThread;
}

/* ── __OSReschedule (OSThread.c:306-313) ── */
static void oracle_OSReschedule(void)
{
    if (!oracle_RunQueueHint) return;
    oracle_SelectThread(0);
}

/* ── __OSUnlockAllMutex (stub) ── */
static void oracle_OSUnlockAllMutex(oracle_OSThread *thread)
{
    (void)thread;
}

/* ════════════════════════════════════════════════════════════════════
 *  Public API functions
 * ════════════════════════════════════════════════════════════════════ */

/* ── __OSThreadInit (OSThread.c:95-128) ── */
static void oracle_OSThreadInit(void)
{
    int prio;
    oracle_OSThread *def;

    memset(oracle_ThreadPool, 0, sizeof(oracle_ThreadPool));
    memset(oracle_ThreadUsed, 0, sizeof(oracle_ThreadUsed));

    /* Set all thread IDs so they're always valid for comparison */
    for (prio = 0; prio < ORACLE_MAX_THREADS; prio++) {
        oracle_ThreadPool[prio].id = prio;
    }

    oracle_CurrentThread = NULL;
    oracle_RunQueueBits = 0;
    oracle_RunQueueHint = 0;
    oracle_Reschedule = 0;

    for (prio = ORACLE_OS_PRIORITY_MIN; prio <= ORACLE_OS_PRIORITY_MAX; prio++) {
        oracle_OSInitThreadQueue(&oracle_RunQueue[prio]);
    }
    oracle_OSInitThreadQueue(&oracle_ActiveThreadQueue);
    for (prio = 0; prio < ORACLE_MAX_WAIT_QUEUES; prio++) {
        oracle_OSInitThreadQueue(&oracle_WaitQueues[prio]);
    }

    /* Default thread (slot 0): RUNNING, priority 16, detached */
    def = &oracle_ThreadPool[0];
    def->id = 0;
    def->state = ORACLE_OS_THREAD_STATE_RUNNING;
    def->attr = ORACLE_OS_THREAD_ATTR_DETACH;
    def->priority = def->base = 16;
    def->suspend = 0;
    def->val = 0xFFFFFFFFu;
    def->queue = NULL;
    oracle_OSInitThreadQueue(&def->queueJoin);
    oracle_ThreadUsed[0] = 1;

    oracle_CurrentThread = def;
    oracle_AddTail(&oracle_ActiveThreadQueue, def, linkActive);
}

/* ── OSCreateThread (OSThread.c:324-359) ── */
static int oracle_OSCreateThread(oracle_OSThread *thread,
                                 int32_t priority, uint16_t attr)
{
    if (priority < 0 || priority > 31) return 0;

    thread->state = ORACLE_OS_THREAD_STATE_READY;
    thread->attr = attr & 1u;
    thread->base = priority;
    thread->priority = priority;
    thread->suspend = 1;
    thread->val = 0xFFFFFFFFu;
    thread->queue = NULL;
    thread->link.next = thread->link.prev = NULL;
    oracle_OSInitThreadQueue(&thread->queueJoin);

    oracle_AddTail(&oracle_ActiveThreadQueue, thread, linkActive);
    return 1;
}

/* ── OSResumeThread (OSThread.c:430-459) ── */
static int32_t oracle_OSResumeThread(oracle_OSThread *thread)
{
    int32_t suspendCount;

    suspendCount = thread->suspend--;
    if (thread->suspend < 0) {
        thread->suspend = 0;
    }
    else if (thread->suspend == 0) {
        switch (thread->state) {
            case ORACLE_OS_THREAD_STATE_READY:
                thread->priority = oracle_OSGetEffectivePriority(thread);
                oracle_SetRun(thread);
                break;
            case ORACLE_OS_THREAD_STATE_WAITING:
                oracle_RemoveItem(thread->queue, thread, link);
                thread->priority = oracle_OSGetEffectivePriority(thread);
                oracle_AddPrio(thread->queue, thread, link);
                break;
        }
        oracle_OSReschedule();
    }
    return suspendCount;
}

/* ── OSSuspendThread (OSThread.c:461-491) ── */
static int32_t oracle_OSSuspendThread(oracle_OSThread *thread)
{
    int32_t suspendCount;

    suspendCount = thread->suspend++;
    if (suspendCount == 0) {
        switch (thread->state) {
            case ORACLE_OS_THREAD_STATE_RUNNING:
                oracle_RunQueueHint = 1;
                thread->state = ORACLE_OS_THREAD_STATE_READY;
                break;
            case ORACLE_OS_THREAD_STATE_READY:
                oracle_UnsetRun(thread);
                break;
            case ORACLE_OS_THREAD_STATE_WAITING:
                oracle_RemoveItem(thread->queue, thread, link);
                thread->priority = 32;
                oracle_AddTail(thread->queue, thread, link);
                break;
        }
        oracle_OSReschedule();
    }
    return suspendCount;
}

/* ── OSSleepThread (OSThread.c:493-507) ── */
static void oracle_OSSleepThread(oracle_OSThreadQueue *queue)
{
    oracle_OSThread *currentThread = oracle_CurrentThread;

    currentThread->state = ORACLE_OS_THREAD_STATE_WAITING;
    currentThread->queue = queue;
    oracle_AddPrio(queue, currentThread, link);
    oracle_RunQueueHint = 1;
    oracle_OSReschedule();
}

/* ── OSWakeupThread (OSThread.c:509-524) ── */
static void oracle_OSWakeupThread(oracle_OSThreadQueue *queue)
{
    oracle_OSThread *thread;

    while (queue->head) {
        oracle_RemoveHead(queue, thread, link);
        thread->state = ORACLE_OS_THREAD_STATE_READY;
        if (!(0 < thread->suspend)) {
            oracle_SetRun(thread);
        }
    }
    oracle_OSReschedule();
}

/* ── OSYieldThread (OSThread.c:315-322) ── */
static void oracle_OSYieldThread(void)
{
    oracle_SelectThread(1);
}

/* ── OSCancelThread (OSThread.c:384-428) ── */
static void oracle_OSCancelThread(oracle_OSThread *thread)
{
    switch (thread->state) {
        case ORACLE_OS_THREAD_STATE_READY:
            if (!(0 < thread->suspend)) {
                oracle_UnsetRun(thread);
            }
            break;
        case ORACLE_OS_THREAD_STATE_RUNNING:
            oracle_RunQueueHint = 1;
            break;
        case ORACLE_OS_THREAD_STATE_WAITING:
            oracle_RemoveItem(thread->queue, thread, link);
            thread->queue = NULL;
            break;
        default:
            return;
    }

    if (thread->attr & ORACLE_OS_THREAD_ATTR_DETACH) {
        oracle_RemoveItem(&oracle_ActiveThreadQueue, thread, linkActive);
        thread->state = 0;
    } else {
        thread->state = ORACLE_OS_THREAD_STATE_MORIBUND;
    }

    oracle_OSUnlockAllMutex(thread);
    oracle_OSWakeupThread(&thread->queueJoin);
    oracle_OSReschedule();
}

/* ── OSExitThread (OSThread.c:361-382) ── */
static void oracle_OSExitThread(uint32_t val)
{
    oracle_OSThread *currentThread = oracle_CurrentThread;

    if (currentThread->attr & ORACLE_OS_THREAD_ATTR_DETACH) {
        oracle_RemoveItem(&oracle_ActiveThreadQueue, currentThread, linkActive);
        currentThread->state = 0;
    } else {
        currentThread->state = ORACLE_OS_THREAD_STATE_MORIBUND;
        currentThread->val = val;
    }

    oracle_OSUnlockAllMutex(currentThread);
    oracle_OSWakeupThread(&currentThread->queueJoin);
    oracle_RunQueueHint = 1;
    oracle_SelectThread(0);
}

/* ── OSDisableScheduler / OSEnableScheduler (OSThread.c:140-160) ── */
static int32_t oracle_OSDisableScheduler(void)
{
    return oracle_Reschedule++;
}

static int32_t oracle_OSEnableScheduler(void)
{
    return oracle_Reschedule--;
}
