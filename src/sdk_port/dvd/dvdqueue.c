/*
 * sdk_port/dvd/dvdqueue.c --- Host-side port of GC SDK dvdqueue.
 *
 * Source of truth: external/mp4-decomp/src/dolphin/dvd/dvdqueue.c
 *
 * Same logic as decomp. DVDCommandBlock structs and queue sentinels
 * live in gc_mem (big-endian). Circular doubly-linked lists with
 * sentinel nodes. Interrupt disable/enable stripped.
 */
#include <stdint.h>
#include "dvdqueue.h"
#include "../gc_mem.h"

/* ── Big-endian u32 helpers ── */

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

/* ── __DVDClearWaitingQueue (dvdqueue.c:14-21) ── */

void port_DVDClearWaitingQueue(port_DVDQueueState *st)
{
    uint32_t i;
    for (i = 0; i < PORT_DVD_MAX_QUEUES; i++) {
        uint32_t q = PORT_DVD_QUEUE_ADDR(st, i);
        store_u32be(q + PORT_DVD_QUEUE_NEXT, q);
        store_u32be(q + PORT_DVD_QUEUE_PREV, q);
    }
}

/* ── __DVDPushWaitingQueue (dvdqueue.c:23-33) ── */

void port_DVDPushWaitingQueue(port_DVDQueueState *st, int32_t prio,
                              uint32_t blockAddr)
{
    uint32_t q = PORT_DVD_QUEUE_ADDR(st, prio);
    uint32_t q_prev = load_u32be(q + PORT_DVD_QUEUE_PREV);

    /* q->prev->next = block */
    store_u32be(q_prev + PORT_DVD_BLOCK_NEXT, blockAddr);
    /* block->prev = q->prev */
    store_u32be(blockAddr + PORT_DVD_BLOCK_PREV, q_prev);
    /* block->next = q */
    store_u32be(blockAddr + PORT_DVD_BLOCK_NEXT, q);
    /* q->prev = block */
    store_u32be(q + PORT_DVD_QUEUE_PREV, blockAddr);
}

/* ── PopWaitingQueuePrio (internal, dvdqueue.c:35-47) ── */

static uint32_t PopWaitingQueuePrio(port_DVDQueueState *st, int32_t prio)
{
    uint32_t q = PORT_DVD_QUEUE_ADDR(st, prio);
    uint32_t tmp = load_u32be(q + PORT_DVD_QUEUE_NEXT);
    uint32_t tmp_next = load_u32be(tmp + PORT_DVD_BLOCK_NEXT);

    /* q->next = tmp->next */
    store_u32be(q + PORT_DVD_QUEUE_NEXT, tmp_next);
    /* tmp->next->prev = q */
    store_u32be(tmp_next + PORT_DVD_BLOCK_PREV, q);

    /* tmp->next = NULL, tmp->prev = NULL */
    store_u32be(tmp + PORT_DVD_BLOCK_NEXT, 0);
    store_u32be(tmp + PORT_DVD_BLOCK_PREV, 0);

    return tmp;
}

/* ── __DVDPopWaitingQueue (dvdqueue.c:49-60) ── */

uint32_t port_DVDPopWaitingQueue(port_DVDQueueState *st)
{
    uint32_t i;
    for (i = 0; i < PORT_DVD_MAX_QUEUES; i++) {
        uint32_t q = PORT_DVD_QUEUE_ADDR(st, i);
        if (load_u32be(q + PORT_DVD_QUEUE_NEXT) != q) {
            return PopWaitingQueuePrio(st, (int32_t)i);
        }
    }
    return 0; /* NULL */
}

/* ── __DVDCheckWaitingQueue (dvdqueue.c:62-70) ── */

int port_DVDCheckWaitingQueue(port_DVDQueueState *st)
{
    uint32_t i;
    for (i = 0; i < PORT_DVD_MAX_QUEUES; i++) {
        uint32_t q = PORT_DVD_QUEUE_ADDR(st, i);
        if (load_u32be(q + PORT_DVD_QUEUE_NEXT) != q) return 1;
    }
    return 0;
}

/* ── __DVDDequeueWaitingQueue (dvdqueue.c:72-82) ── */

int port_DVDDequeueWaitingQueue(uint32_t blockAddr)
{
    uint32_t prev = load_u32be(blockAddr + PORT_DVD_BLOCK_PREV);
    uint32_t next = load_u32be(blockAddr + PORT_DVD_BLOCK_NEXT);

    if (prev == 0 || next == 0) return 0;

    /* prev->next = next */
    store_u32be(prev + PORT_DVD_BLOCK_NEXT, next);
    /* next->prev = prev */
    store_u32be(next + PORT_DVD_BLOCK_PREV, prev);

    return 1;
}

/* ── __DVDIsBlockInWaitingQueue (dvdqueue.c:84-97) ── */

int port_DVDIsBlockInWaitingQueue(port_DVDQueueState *st, uint32_t blockAddr)
{
    uint32_t i;
    for (i = 0; i < PORT_DVD_MAX_QUEUES; i++) {
        uint32_t sentinel = PORT_DVD_QUEUE_ADDR(st, i);
        if (load_u32be(sentinel + PORT_DVD_QUEUE_NEXT) != sentinel) {
            uint32_t q;
            for (q = load_u32be(sentinel + PORT_DVD_QUEUE_NEXT);
                 q != sentinel;
                 q = load_u32be(q + PORT_DVD_BLOCK_NEXT)) {
                if (q == blockAddr) return 1;
            }
        }
    }
    return 0;
}
