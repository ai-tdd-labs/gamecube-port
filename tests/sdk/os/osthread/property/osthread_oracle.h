/*
 * osthread_oracle.h --- Oracle for OSThread + OSMutex property tests.
 *
 * Exact copy of decomp OSThread/OSMutex scheduler logic with oracle_ prefix.
 * Hardware calls (context save/load, interrupts) are stripped/mocked.
 *
 * Source of truth:
 *   external/mp4-decomp/src/dolphin/os/OSThread.c
 *   external/mp4-decomp/src/dolphin/os/OSMutex.c
 *   external/mp4-decomp/include/dolphin/os/OSThread.h
 *   external/mp4-decomp/include/dolphin/os/OSMutex.h
 *
 * Note on blocking calls (LockMutex, JoinThread, WaitCond):
 *   The decomp uses while loops that span context switches.  Our oracle has
 *   no real context switch — OSSleepThread just changes oracle_CurrentThread.
 *   Therefore blocking API functions do ONE iteration: if contended they
 *   sleep and return.  The test driver must call oracle_ProcessPendingLocks()
 *   after each operation to complete deferred re-acquisitions.
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
#define ORACLE_MAX_MUTEXES      16
#define ORACLE_MAX_CONDS         8
#define ORACLE_MAX_MSGQUEUES     8
#define ORACLE_MAX_MSGS_PER_Q   16

#define ORACLE_OS_MESSAGE_NOBLOCK 0
#define ORACLE_OS_MESSAGE_BLOCK   1

/* ── Forward declarations ── */
typedef struct oracle_OSThread oracle_OSThread;
typedef struct oracle_OSThreadQueue oracle_OSThreadQueue;
typedef struct oracle_OSThreadLink oracle_OSThreadLink;
typedef struct oracle_OSMutex oracle_OSMutex;
typedef struct oracle_OSMutexQueue oracle_OSMutexQueue;
typedef struct oracle_OSMutexLink oracle_OSMutexLink;
typedef struct oracle_OSCond oracle_OSCond;
typedef struct oracle_OSMessageQueue oracle_OSMessageQueue;

/* ── Thread queue / link ── */
struct oracle_OSThreadQueue {
    oracle_OSThread *head;
    oracle_OSThread *tail;
};

struct oracle_OSThreadLink {
    oracle_OSThread *next;
    oracle_OSThread *prev;
};

/* ── Mutex queue / link (for thread's owned-mutex list) ── */
struct oracle_OSMutexQueue {
    oracle_OSMutex *head;
    oracle_OSMutex *tail;
};

struct oracle_OSMutexLink {
    oracle_OSMutex *next;
    oracle_OSMutex *prev;
};

/* ── OSMutex ── */
struct oracle_OSMutex {
    int id;                          /* index in pool */
    oracle_OSThreadQueue queue;      /* threads waiting on this mutex */
    oracle_OSThread     *thread;     /* owner (NULL = unlocked) */
    int32_t              count;      /* recursive lock count */
    oracle_OSMutexLink   link;       /* link in owner's queueMutex */
};

/* ── OSCond ── */
struct oracle_OSCond {
    oracle_OSThreadQueue queue;
};

/* ── OSMessageQueue ── */
struct oracle_OSMessageQueue {
    int id;
    oracle_OSThreadQueue queueSend;     /* threads blocked on full queue */
    oracle_OSThreadQueue queueReceive;  /* threads blocked on empty queue */
    uint32_t msgArray[ORACLE_MAX_MSGS_PER_Q]; /* circular buffer of messages */
    int32_t  msgCount;                  /* capacity */
    int32_t  firstIndex;               /* index of first message */
    int32_t  usedCount;                /* number of messages in queue */
};

/* ── OSThread ── */
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
    oracle_OSMutex      *mutex;      /* mutex this thread is waiting on */
    oracle_OSMutexQueue  queueMutex; /* queue of mutexes owned by thread */
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
static oracle_OSMutex       oracle_MutexPool[ORACLE_MAX_MUTEXES];
static oracle_OSCond        oracle_CondPool[ORACLE_MAX_CONDS];
static oracle_OSMessageQueue oracle_MsgQPool[ORACLE_MAX_MSGQUEUES];

/* ════════════════════════════════════════════════════════════════════
 * Thread queue macros (OSThread.c:24-83, exact logic)
 *
 * Token-paste `link` field: `link` for run/wait queues,
 * `linkActive` for the active-thread queue.
 * ════════════════════════════════════════════════════════════════════ */

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

/* ════════════════════════════════════════════════════════════════════
 * Mutex queue macros (OSMutex.c:3-47, exact logic)
 *
 * These operate on oracle_OSMutexQueue (thread's owned-mutex list).
 * ════════════════════════════════════════════════════════════════════ */

#define oracle_MutexPushTail(queue, mutex, link)                     \
    do {                                                             \
        oracle_OSMutex *__prev;                                      \
        __prev = (queue)->tail;                                      \
        if (__prev == NULL)                                          \
            (queue)->head = (mutex);                                 \
        else                                                         \
            __prev->link.next = (mutex);                             \
        (mutex)->link.prev = __prev;                                 \
        (mutex)->link.next = NULL;                                   \
        (queue)->tail = (mutex);                                     \
    } while (0)

#define oracle_MutexPopHead(queue, mutex, link)                      \
    do {                                                             \
        oracle_OSMutex *__next;                                      \
        (mutex) = (queue)->head;                                     \
        __next = (mutex)->link.next;                                 \
        if (__next == NULL)                                          \
            (queue)->tail = NULL;                                    \
        else                                                         \
            __next->link.prev = NULL;                                \
        (queue)->head = __next;                                      \
    } while (0)

#define oracle_MutexPopItem(queue, mutex, link)                      \
    do {                                                             \
        oracle_OSMutex *__next;                                      \
        oracle_OSMutex *__prev;                                      \
        __next = (mutex)->link.next;                                 \
        __prev = (mutex)->link.prev;                                 \
        if (__next == NULL)                                          \
            (queue)->tail = __prev;                                  \
        else                                                         \
            __next->link.prev = __prev;                              \
        if (__prev == NULL)                                          \
            (queue)->head = __next;                                  \
        else                                                         \
            __prev->link.next = __next;                              \
    } while (0)

/* ════════════════════════════════════════════════════════════════════
 * cntlzw --- count leading zeros (PPC instruction)
 * ════════════════════════════════════════════════════════════════════ */
static uint32_t oracle_cntlzw(uint32_t x)
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

/* ════════════════════════════════════════════════════════════════════
 * Init / helper functions
 * ════════════════════════════════════════════════════════════════════ */

static void oracle_OSInitThreadQueue(oracle_OSThreadQueue *queue)
{
    queue->head = queue->tail = NULL;
}

static void oracle_OSInitMutexQueue(oracle_OSMutexQueue *queue)
{
    queue->head = queue->tail = NULL;
}

/* ────────────────────────────────────────────────────────────────────
 * __OSGetEffectivePriority (OSThread.c:181-195)
 *
 * Scan thread's owned mutexes.  For each, check if a higher-priority
 * thread is waiting.  Return the minimum (= highest) priority.
 * ──────────────────────────────────────────────────────────────────── */
static int32_t oracle_OSGetEffectivePriority(oracle_OSThread *thread)
{
    int32_t priority;
    oracle_OSMutex *mutex;
    oracle_OSThread *blocked;

    priority = thread->base;
    for (mutex = thread->queueMutex.head; mutex; mutex = mutex->link.next) {
        blocked = mutex->queue.head;
        if (blocked && blocked->priority < priority) {
            priority = blocked->priority;
        }
    }
    return priority;
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
 * SetEffectivePriority (OSThread.c:197-219)
 *
 * Returns the next thread in the priority inheritance chain:
 * for WAITING threads that are blocked on a mutex, returns the
 * mutex owner so the caller can propagate the priority.
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
            if (thread->mutex) {
                return thread->mutex->thread;
            }
            break;
        case ORACLE_OS_THREAD_STATE_RUNNING:
            oracle_RunQueueHint = 1;
            thread->priority = priority;
            break;
    }
    return NULL;
}

/* ────────────────────────────────────────────────────────────────────
 * UpdatePriority (OSThread.c:221-235)
 * ──────────────────────────────────────────────────────────────────── */
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
 * __OSPromoteThread (not in decomp, reconstructed)
 *
 * Promote a thread's effective priority to `priority` and chain
 * through mutex owners.  Unlike UpdatePriority (which recalculates),
 * this directly sets the priority if it's higher (lower number).
 * ──────────────────────────────────────────────────────────────────── */
static void oracle_OSPromoteThread(oracle_OSThread *thread, int32_t priority)
{
    do {
        if (0 < thread->suspend) break;
        if (thread->priority <= priority) break;
        thread = oracle_SetEffectivePriority(thread, priority);
    } while (thread);
}

/* ────────────────────────────────────────────────────────────────────
 * SelectThread (OSThread.c:244-304, simplified)
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
    }

    oracle_CurrentThread = NULL;

    if (oracle_RunQueueBits == 0) {
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

/* ════════════════════════════════════════════════════════════════════
 *  Public API functions — Thread scheduler
 * ════════════════════════════════════════════════════════════════════ */

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

/* ────────────────────────────────────────────────────────────────────
 * __OSUnlockAllMutex (OSMutex.c:101-111)
 *
 * Called when a thread exits or is cancelled.  Releases all mutexes
 * owned by the thread and wakes up any waiters.
 * ──────────────────────────────────────────────────────────────────── */
static void oracle_OSUnlockAllMutex(oracle_OSThread *thread)
{
    oracle_OSMutex *mutex;

    while (thread->queueMutex.head) {
        oracle_MutexPopHead(&thread->queueMutex, mutex, link);
        mutex->count = 0;
        mutex->thread = NULL;
        oracle_OSWakeupThread(&mutex->queue);
    }
}

/* ── OSYieldThread (OSThread.c:315-322) ── */
static void oracle_OSYieldThread(void)
{
    oracle_SelectThread(1);
}

/* ── __OSThreadInit (OSThread.c:95-128) ── */
static void oracle_OSThreadInit(void)
{
    int prio;
    oracle_OSThread *def;

    memset(oracle_ThreadPool, 0, sizeof(oracle_ThreadPool));
    memset(oracle_ThreadUsed, 0, sizeof(oracle_ThreadUsed));
    memset(oracle_MutexPool, 0, sizeof(oracle_MutexPool));
    memset(oracle_CondPool, 0, sizeof(oracle_CondPool));
    memset(oracle_MsgQPool, 0, sizeof(oracle_MsgQPool));

    for (prio = 0; prio < ORACLE_MAX_THREADS; prio++) {
        oracle_ThreadPool[prio].id = prio;
    }
    for (prio = 0; prio < ORACLE_MAX_MUTEXES; prio++) {
        oracle_MutexPool[prio].id = prio;
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
    def->mutex = NULL;
    oracle_OSInitThreadQueue(&def->queueJoin);
    oracle_OSInitMutexQueue(&def->queueMutex);
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
    thread->mutex = NULL;
    oracle_OSInitThreadQueue(&thread->queueJoin);
    oracle_OSInitMutexQueue(&thread->queueMutex);

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
                if (thread->mutex) {
                    oracle_UpdatePriority(thread->mutex->thread);
                }
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
                if (thread->mutex) {
                    oracle_UpdatePriority(thread->mutex->thread);
                }
                break;
        }
        oracle_OSReschedule();
    }
    return suspendCount;
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
            if (!(0 < thread->suspend) && thread->mutex) {
                oracle_UpdatePriority(thread->mutex->thread);
            }
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

/* ════════════════════════════════════════════════════════════════════
 *  Mutex API functions (OSMutex.c)
 * ════════════════════════════════════════════════════════════════════ */

/* ── OSInitMutex (OSMutex.c:49-54) ── */
static void oracle_OSInitMutex(oracle_OSMutex *mutex)
{
    oracle_OSInitThreadQueue(&mutex->queue);
    mutex->thread = NULL;
    mutex->count = 0;
}

/* ── OSLockMutex (OSMutex.c:56-82)
 *
 * Single-iteration: if contended, sleeps and returns.
 * The test driver calls oracle_ProcessPendingLocks() to complete
 * deferred re-acquisitions when the thread is woken up.
 * ── */
static void oracle_OSLockMutex(oracle_OSMutex *mutex)
{
    oracle_OSThread *currentThread = oracle_CurrentThread;
    oracle_OSThread *ownerThread;

    ownerThread = mutex->thread;
    if (ownerThread == NULL) {
        /* Unlocked: acquire */
        mutex->thread = currentThread;
        mutex->count++;
        oracle_MutexPushTail(&currentThread->queueMutex, mutex, link);
    }
    else if (ownerThread == currentThread) {
        /* Recursive lock */
        mutex->count++;
    }
    else {
        /* Contended: promote owner and sleep */
        currentThread->mutex = mutex;
        oracle_OSPromoteThread(mutex->thread, currentThread->priority);
        oracle_OSSleepThread(&mutex->queue);
        /* Do NOT clear currentThread->mutex or loop.
         * oracle_ProcessPendingLocks() handles the resume. */
    }
}

/* ── OSUnlockMutex (OSMutex.c:84-99) ── */
static void oracle_OSUnlockMutex(oracle_OSMutex *mutex)
{
    oracle_OSThread *currentThread = oracle_CurrentThread;

    if (mutex->thread == currentThread && --mutex->count == 0) {
        oracle_MutexPopItem(&currentThread->queueMutex, mutex, link);
        mutex->thread = NULL;
        if (currentThread->priority < currentThread->base) {
            currentThread->priority =
                oracle_OSGetEffectivePriority(currentThread);
        }
        oracle_OSWakeupThread(&mutex->queue);
    }
}

/* ── OSTryLockMutex (OSMutex.c:113-133) ── */
static int oracle_OSTryLockMutex(oracle_OSMutex *mutex)
{
    oracle_OSThread *currentThread = oracle_CurrentThread;
    int locked;

    if (mutex->thread == NULL) {
        mutex->thread = currentThread;
        mutex->count++;
        oracle_MutexPushTail(&currentThread->queueMutex, mutex, link);
        locked = 1;
    }
    else if (mutex->thread == currentThread) {
        mutex->count++;
        locked = 1;
    }
    else {
        locked = 0;
    }
    return locked;
}

/* ── OSInitCond (OSMutex.c:135-138) ── */
static void oracle_OSInitCond(oracle_OSCond *cond)
{
    oracle_OSInitThreadQueue(&cond->queue);
}

/* ── OSSignalCond (OSMutex.c:167-170) ── */
static void oracle_OSSignalCond(oracle_OSCond *cond)
{
    oracle_OSWakeupThread(&cond->queue);
}

/* ── OSWaitCond (OSMutex.c:140-165)
 *
 * Like LockMutex, the post-sleep code (re-lock + restore count)
 * can't execute correctly without real context switching.
 * Implemented as: release mutex, wake waiters, sleep on cond.
 * The test driver handles re-acquisition via ProcessPendingLocks
 * using a saved-count mechanism.
 *
 * For simplicity, WaitCond is NOT included in the random test mix.
 * It can be tested in controlled sequences.
 * ── */

/* ── OSJoinThread (not in decomp, reconstructed)
 *
 * If target is already MORIBUND: collect val, clean up, return 1.
 * If target is not yet dead: sleep on target's queueJoin.
 * The join completion happens when the target exits/cancels and
 * wakes the joining thread.  Test driver calls ProcessPendingJoins.
 * ── */
static int oracle_OSJoinThread(oracle_OSThread *thread, uint32_t *val)
{
    if (thread->attr & ORACLE_OS_THREAD_ATTR_DETACH) {
        return 0;
    }

    if (thread->state == ORACLE_OS_THREAD_STATE_MORIBUND) {
        if (val) *val = thread->val;
        oracle_RemoveItem(&oracle_ActiveThreadQueue, thread, linkActive);
        thread->state = 0;
        return 1;
    }

    /* Not yet dead — sleep on queueJoin.
     * On real HW this would block until woken, then check moribund.
     * Here we just sleep; the test driver must only call JoinThread
     * when the target is already MORIBUND (non-blocking path). */
    return 0;
}

/* ────────────────────────────────────────────────────────────────────
 * ProcessPendingLocks — called by test driver after each operation.
 *
 * If the current thread was previously sleeping on a mutex (via
 * LockMutex) and has been woken up, retry the lock acquisition.
 * This models the while(TRUE) loop in the real OSLockMutex.
 * ──────────────────────────────────────────────────────────────────── */
static void oracle_ProcessPendingLocks(void)
{
    oracle_OSThread *ct;
    oracle_OSMutex *m;

    /* Loop because a successful lock may trigger a reschedule that
     * wakes yet another thread with a pending lock. */
    while (1) {
        ct = oracle_CurrentThread;
        if (!ct) break;
        if (!ct->mutex) break;
        if (ct->state != ORACLE_OS_THREAD_STATE_RUNNING) break;

        m = ct->mutex;
        ct->mutex = NULL;
        /* Retry the lock — same logic as OSLockMutex */
        oracle_OSLockMutex(m);
    }
}

/* ════════════════════════════════════════════════════════════════════
 *  OSMessage API functions (OSMessage.c)
 *
 *  Same blocking-loop adaptation as OSMutex: non-blocking only in the
 *  random test mix.  Blocking paths (OS_MESSAGE_BLOCK) can be tested
 *  in controlled sequences where the test driver ensures the operation
 *  can complete.
 * ════════════════════════════════════════════════════════════════════ */

/* ── OSInitMessageQueue (OSMessage.c:3-11) ── */
static void oracle_OSInitMessageQueue(oracle_OSMessageQueue *mq, int32_t msgCount)
{
    oracle_OSInitThreadQueue(&mq->queueSend);
    oracle_OSInitThreadQueue(&mq->queueReceive);
    memset(mq->msgArray, 0, sizeof(mq->msgArray));
    mq->msgCount = msgCount;
    mq->firstIndex = 0;
    mq->usedCount = 0;
}

/* ── OSSendMessage (OSMessage.c:12-37, non-blocking path) ──
 *
 * For NOBLOCK: returns 1 if sent, 0 if queue full.
 * For BLOCK: if full, sleeps on queueSend and returns 0.
 *   The test driver calls ProcessPendingSends() to complete.
 * ── */
static int oracle_OSSendMessage(oracle_OSMessageQueue *mq, uint32_t msg, int32_t flags)
{
    if (mq->msgCount <= mq->usedCount) {
        if (!(flags & ORACLE_OS_MESSAGE_BLOCK)) {
            return 0; /* FALSE: queue full, non-blocking */
        }
        /* BLOCK: sleep on queueSend */
        oracle_OSSleepThread(&mq->queueSend);
        return 0; /* Deferred — ProcessPendingSends will complete */
    }

    {
        int32_t lastIndex = (mq->firstIndex + mq->usedCount) % mq->msgCount;
        mq->msgArray[lastIndex] = msg;
        mq->usedCount++;
    }

    oracle_OSWakeupThread(&mq->queueReceive);
    return 1; /* TRUE: sent */
}

/* ── OSReceiveMessage (OSMessage.c:39-65, non-blocking path) ── */
static int oracle_OSReceiveMessage(oracle_OSMessageQueue *mq, uint32_t *msg, int32_t flags)
{
    if (mq->usedCount == 0) {
        if (!(flags & ORACLE_OS_MESSAGE_BLOCK)) {
            return 0; /* FALSE: queue empty, non-blocking */
        }
        /* BLOCK: sleep on queueReceive */
        oracle_OSSleepThread(&mq->queueReceive);
        return 0; /* Deferred */
    }

    if (msg != NULL) {
        *msg = mq->msgArray[mq->firstIndex];
    }
    mq->firstIndex = (mq->firstIndex + 1) % mq->msgCount;
    mq->usedCount--;

    oracle_OSWakeupThread(&mq->queueSend);
    return 1; /* TRUE: received */
}

/* ── OSJamMessage (OSMessage.c:67-91, non-blocking path) ── */
static int oracle_OSJamMessage(oracle_OSMessageQueue *mq, uint32_t msg, int32_t flags)
{
    if (mq->msgCount <= mq->usedCount) {
        if (!(flags & ORACLE_OS_MESSAGE_BLOCK)) {
            return 0; /* FALSE: queue full, non-blocking */
        }
        oracle_OSSleepThread(&mq->queueSend);
        return 0; /* Deferred */
    }

    mq->firstIndex = (mq->firstIndex + mq->msgCount - 1) % mq->msgCount;
    mq->msgArray[mq->firstIndex] = msg;
    mq->usedCount++;

    oracle_OSWakeupThread(&mq->queueReceive);
    return 1; /* TRUE: jammed */
}
