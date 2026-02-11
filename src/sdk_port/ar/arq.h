/*
 * sdk_port/ar/arq.h --- Host-side port of GC SDK ARQ (ARAM Request Queue).
 *
 * ARQRequest structs live in gc_mem (big-endian). Queue pointers are
 * GC addresses (u32). DMA is mocked via a log.
 */
#pragma once

#include <stdint.h>

/* Constants */
#define PORT_ARQ_TYPE_MRAM_TO_ARAM  0
#define PORT_ARQ_TYPE_ARAM_TO_MRAM  1
#define PORT_ARQ_PRIORITY_LOW       0
#define PORT_ARQ_PRIORITY_HIGH      1
#define PORT_ARQ_CHUNK_SIZE_DEFAULT 4096

/* ARQRequest field offsets in gc_mem (8 x u32 = 32 bytes) */
#define PORT_ARQ_REQ_NEXT     0
#define PORT_ARQ_REQ_OWNER    4
#define PORT_ARQ_REQ_TYPE     8
#define PORT_ARQ_REQ_PRIORITY 12
#define PORT_ARQ_REQ_SOURCE   16
#define PORT_ARQ_REQ_DEST     20
#define PORT_ARQ_REQ_LENGTH   24
#define PORT_ARQ_REQ_CALLBACK 28
#define PORT_ARQ_REQ_SIZE     32

/* DMA record */
typedef struct {
    uint32_t type;
    uint32_t mainmem;
    uint32_t aram;
    uint32_t length;
} port_DMARecord;

#define PORT_MAX_DMA 2048

/* ARQ state (host-side, queue pointers are GC addresses) */
typedef struct {
    uint32_t queueHi;     /* GC addr of Hi queue head (0 = NULL) */
    uint32_t tailHi;      /* GC addr of Hi queue tail */
    uint32_t queueLo;     /* GC addr of Lo queue head */
    uint32_t tailLo;      /* GC addr of Lo queue tail */
    uint32_t pendingHi;   /* GC addr of pending Hi request */
    uint32_t pendingLo;   /* GC addr of pending Lo request */
    int      hasCallbackHi;
    int      hasCallbackLo;
    uint32_t chunkSize;
    /* DMA log */
    port_DMARecord dma_log[PORT_MAX_DMA];
    int            dma_count;
    int            callback_count;
} port_ARQState;

void port_ARQInit(port_ARQState *st);
void port_ARQPostRequest(port_ARQState *st, uint32_t req_addr,
                         uint32_t owner, uint32_t type, uint32_t priority,
                         uint32_t source, uint32_t dest,
                         uint32_t length, int has_callback);
void port_ARQPopTaskQueueHi(port_ARQState *st);
void port_ARQServiceQueueLo(port_ARQState *st);
void port_ARQInterruptServiceRoutine(port_ARQState *st);
