/*
 * sdk_port/dvd/dvdqueue.h --- Host-side port of GC SDK dvdqueue.
 *
 * DVDCommandBlock and queue sentinel structs live in gc_mem (big-endian).
 * Queue pointers are GC addresses (u32). Circular doubly-linked lists
 * with sentinel nodes (empty queue: sentinel.next == sentinel.prev == sentinel).
 *
 * Source of truth: external/mp4-decomp/src/dolphin/dvd/dvdqueue.c
 */
#pragma once

#include <stdint.h>

/* Queue sentinel field offsets (8 bytes) */
#define PORT_DVD_QUEUE_NEXT   0
#define PORT_DVD_QUEUE_PREV   4
#define PORT_DVD_QUEUE_SIZE   8

/* DVDCommandBlock field offsets (minimum for queue ops, 12 bytes) */
#define PORT_DVD_BLOCK_NEXT      0
#define PORT_DVD_BLOCK_PREV      4
#define PORT_DVD_BLOCK_COMMAND   8   /* u32: decomp's 'command' field; tests use for id */
#define PORT_DVD_BLOCK_SIZE     12

#define PORT_DVD_MAX_QUEUES  4

/* Host-side state: base GC address of 4 queue sentinels */
typedef struct {
    uint32_t queuesBase;  /* GC addr where 4 queue sentinels start (4 * 8 = 32 bytes) */
} port_DVDQueueState;

/* Derive individual queue sentinel address */
#define PORT_DVD_QUEUE_ADDR(st, i) \
    ((st)->queuesBase + (uint32_t)(i) * PORT_DVD_QUEUE_SIZE)

void port_DVDClearWaitingQueue(port_DVDQueueState *st);
void port_DVDPushWaitingQueue(port_DVDQueueState *st, int32_t prio,
                              uint32_t blockAddr);
uint32_t port_DVDPopWaitingQueue(port_DVDQueueState *st);
int  port_DVDCheckWaitingQueue(port_DVDQueueState *st);
int  port_DVDDequeueWaitingQueue(uint32_t blockAddr);
int  port_DVDIsBlockInWaitingQueue(port_DVDQueueState *st, uint32_t blockAddr);
