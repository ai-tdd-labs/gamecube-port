/*
 * sdk_port/os/osthread.h --- Host-side port of GC SDK OSThread scheduler.
 *
 * Thread structs and queues live in gc_mem (big-endian).
 * Queue pointers are GC addresses (u32, 0 = NULL).
 * Context save/load, interrupts, and mutexes are stripped/stubbed.
 */
#pragma once

#include <stdint.h>

/* ── Constants ── */
#define PORT_OS_THREAD_STATE_READY    1
#define PORT_OS_THREAD_STATE_RUNNING  2
#define PORT_OS_THREAD_STATE_WAITING  4
#define PORT_OS_THREAD_STATE_MORIBUND 8

#define PORT_OS_THREAD_ATTR_DETACH 0x0001u

#define PORT_OS_PRIORITY_MIN 0
#define PORT_OS_PRIORITY_MAX 31

/* ── Thread struct field offsets in gc_mem ──
 * Compact layout (no OSContext, no stack pointers): */
#define PORT_THREAD_ID           0    /* u32: thread index */
#define PORT_THREAD_STATE        4    /* u16 */
#define PORT_THREAD_ATTR         6    /* u16 */
#define PORT_THREAD_SUSPEND      8    /* s32 */
#define PORT_THREAD_PRIORITY    12    /* s32 (effective) */
#define PORT_THREAD_BASE        16    /* s32 (base) */
#define PORT_THREAD_VAL         20    /* u32 */
#define PORT_THREAD_QUEUE       24    /* u32 (GC addr of queue) */
#define PORT_THREAD_LINK_NEXT   28    /* u32 */
#define PORT_THREAD_LINK_PREV   32    /* u32 */
#define PORT_THREAD_JOIN_HEAD   36    /* u32 */
#define PORT_THREAD_JOIN_TAIL   40    /* u32 */
#define PORT_THREAD_ACTIVE_NEXT 44    /* u32 */
#define PORT_THREAD_ACTIVE_PREV 48    /* u32 */
#define PORT_THREAD_SIZE        52

/* ── Queue struct in gc_mem ── */
#define PORT_QUEUE_HEAD  0    /* u32 */
#define PORT_QUEUE_TAIL  4    /* u32 */
#define PORT_QUEUE_SIZE  8

/* ── Max counts ── */
#define PORT_MAX_THREADS      32
#define PORT_MAX_WAIT_QUEUES  16

/* ── Memory layout in gc_mem ──
 *
 * BASE + 0:                                 RunQueue[32]      = 32 * 8 = 256
 * BASE + 256:                               ActiveQueue       = 8
 * BASE + 264:                               Thread[MAX]       = 32 * 52 = 1664
 * BASE + 1928:                              WaitQueue[MAX]    = 16 * 8 = 128
 * Total: 2056 bytes
 */
#define PORT_RUNQUEUE_OFFSET    0
#define PORT_ACTIVEQ_OFFSET     (PORT_RUNQUEUE_OFFSET + 32 * PORT_QUEUE_SIZE)
#define PORT_THREADS_OFFSET     (PORT_ACTIVEQ_OFFSET + PORT_QUEUE_SIZE)
#define PORT_WAITQ_OFFSET       (PORT_THREADS_OFFSET + PORT_MAX_THREADS * PORT_THREAD_SIZE)
#define PORT_TOTAL_SIZE         (PORT_WAITQ_OFFSET + PORT_MAX_WAIT_QUEUES * PORT_QUEUE_SIZE)

/* ── Port state (host-side) ── */
typedef struct {
    uint32_t currentThread;   /* GC addr of current thread, 0 = NULL */
    uint32_t runQueueBits;
    int32_t  runQueueHint;
    int32_t  reschedule;
    /* GC base address for the entire block */
    uint32_t gc_base;
} port_OSThreadState;

/* Get GC address of a queue or thread from its index */
#define PORT_RUNQUEUE_ADDR(st, prio) \
    ((st)->gc_base + PORT_RUNQUEUE_OFFSET + (uint32_t)(prio) * PORT_QUEUE_SIZE)
#define PORT_ACTIVEQ_ADDR(st) \
    ((st)->gc_base + PORT_ACTIVEQ_OFFSET)
#define PORT_THREAD_ADDR(st, idx) \
    ((st)->gc_base + PORT_THREADS_OFFSET + (uint32_t)(idx) * PORT_THREAD_SIZE)
#define PORT_WAITQ_ADDR(st, idx) \
    ((st)->gc_base + PORT_WAITQ_OFFSET + (uint32_t)(idx) * PORT_QUEUE_SIZE)

/* ── API ── */
void port_OSThreadInit(port_OSThreadState *st, uint32_t gc_base);
int  port_OSCreateThread(port_OSThreadState *st, int slot,
                         int32_t priority, uint16_t attr);
int32_t port_OSResumeThread(port_OSThreadState *st, uint32_t thread_addr);
int32_t port_OSSuspendThread(port_OSThreadState *st, uint32_t thread_addr);
void    port_OSSleepThread(port_OSThreadState *st, uint32_t queue_addr);
void    port_OSWakeupThread(port_OSThreadState *st, uint32_t queue_addr);
void    port_OSYieldThread(port_OSThreadState *st);
void    port_OSCancelThread(port_OSThreadState *st, uint32_t thread_addr);
void    port_OSExitThread(port_OSThreadState *st, uint32_t val);
int32_t port_OSDisableScheduler(port_OSThreadState *st);
int32_t port_OSEnableScheduler(port_OSThreadState *st);
