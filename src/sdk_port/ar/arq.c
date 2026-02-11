/*
 * sdk_port/ar/arq.c --- Host-side port of GC SDK ARQ.
 *
 * Source of truth: external/mp4-decomp/src/dolphin/ar/arq.c
 *
 * Same logic as decomp, but ARQRequest structs live in gc_mem (big-endian).
 * Queue pointers are GC addresses (u32, 0 = NULL).
 * ARStartDMA is mocked: records DMA params in a log.
 */
#include <stdint.h>
#include "arq.h"
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

/* Convenience: read/write ARQRequest fields */
static inline uint32_t req_get(uint32_t req, int field)
{
    return load_u32be(req + (uint32_t)field);
}

static inline void req_set(uint32_t req, int field, uint32_t val)
{
    store_u32be(req + (uint32_t)field, val);
}

/* ── Mock ARStartDMA ── */

static void port_ARStartDMA(port_ARQState *st, uint32_t type,
                             uint32_t mainmem, uint32_t aram, uint32_t length)
{
    if (st->dma_count < PORT_MAX_DMA) {
        st->dma_log[st->dma_count].type    = type;
        st->dma_log[st->dma_count].mainmem = mainmem;
        st->dma_log[st->dma_count].aram    = aram;
        st->dma_log[st->dma_count].length  = length;
        st->dma_count++;
    }
}

/* ── Init ── */

void port_ARQInit(port_ARQState *st)
{
    st->queueHi = st->tailHi = 0;
    st->queueLo = st->tailLo = 0;
    st->pendingHi = st->pendingLo = 0;
    st->hasCallbackHi = 0;
    st->hasCallbackLo = 0;
    st->chunkSize = PORT_ARQ_CHUNK_SIZE_DEFAULT;
    st->dma_count = 0;
    st->callback_count = 0;
}

/* ────────────────────────────────────────────────────────────────────
 * __ARQPopTaskQueueHi  (arq.c:23-40)
 * ──────────────────────────────────────────────────────────────────── */
void port_ARQPopTaskQueueHi(port_ARQState *st)
{
    if (st->queueHi) {
        uint32_t req = st->queueHi;
        uint32_t type   = req_get(req, PORT_ARQ_REQ_TYPE);
        uint32_t source = req_get(req, PORT_ARQ_REQ_SOURCE);
        uint32_t dest   = req_get(req, PORT_ARQ_REQ_DEST);
        uint32_t length = req_get(req, PORT_ARQ_REQ_LENGTH);

        if (type == PORT_ARQ_TYPE_MRAM_TO_ARAM) {
            port_ARStartDMA(st, type, source, dest, length);
        } else {
            port_ARStartDMA(st, type, dest, source, length);
        }

        st->hasCallbackHi = 1;
        st->pendingHi = req;
        st->queueHi = req_get(req, PORT_ARQ_REQ_NEXT);
    }
}

/* ────────────────────────────────────────────────────────────────────
 * __ARQServiceQueueLo  (arq.c:42-73)
 * ──────────────────────────────────────────────────────────────────── */
void port_ARQServiceQueueLo(port_ARQState *st)
{
    if ((st->pendingLo == 0) && (st->queueLo)) {
        st->pendingLo = st->queueLo;
        st->queueLo = req_get(st->queueLo, PORT_ARQ_REQ_NEXT);
    }

    if (st->pendingLo) {
        uint32_t req    = st->pendingLo;
        uint32_t type   = req_get(req, PORT_ARQ_REQ_TYPE);
        uint32_t source = req_get(req, PORT_ARQ_REQ_SOURCE);
        uint32_t dest   = req_get(req, PORT_ARQ_REQ_DEST);
        uint32_t length = req_get(req, PORT_ARQ_REQ_LENGTH);

        if (length <= st->chunkSize) {
            if (type == PORT_ARQ_TYPE_MRAM_TO_ARAM)
                port_ARStartDMA(st, type, source, dest, length);
            else
                port_ARStartDMA(st, type, dest, source, length);

            st->hasCallbackLo = 1;
        } else {
            if (type == PORT_ARQ_TYPE_MRAM_TO_ARAM)
                port_ARStartDMA(st, type, source, dest, st->chunkSize);
            else
                port_ARStartDMA(st, type, dest, source, st->chunkSize);
        }

        req_set(req, PORT_ARQ_REQ_LENGTH, length - st->chunkSize);
        req_set(req, PORT_ARQ_REQ_SOURCE, source + st->chunkSize);
        req_set(req, PORT_ARQ_REQ_DEST,   dest + st->chunkSize);
    }
}

/* ────────────────────────────────────────────────────────────────────
 * __ARQInterruptServiceRoutine  (arq.c:76-94)
 * ──────────────────────────────────────────────────────────────────── */
void port_ARQInterruptServiceRoutine(port_ARQState *st)
{
    if (st->hasCallbackHi) {
        st->callback_count++;
        st->pendingHi = 0;
        st->hasCallbackHi = 0;
    }
    else if (st->hasCallbackLo) {
        st->callback_count++;
        st->pendingLo = 0;
        st->hasCallbackLo = 0;
    }

    port_ARQPopTaskQueueHi(st);

    if (st->pendingHi == 0)
        port_ARQServiceQueueLo(st);
}

/* ────────────────────────────────────────────────────────────────────
 * ARQPostRequest  (arq.c:112-166)
 * ──────────────────────────────────────────────────────────────────── */
void port_ARQPostRequest(port_ARQState *st, uint32_t req_addr,
                         uint32_t owner, uint32_t type, uint32_t priority,
                         uint32_t source, uint32_t dest,
                         uint32_t length, int has_callback)
{
    req_set(req_addr, PORT_ARQ_REQ_NEXT, 0);
    req_set(req_addr, PORT_ARQ_REQ_OWNER, owner);
    req_set(req_addr, PORT_ARQ_REQ_TYPE, type);
    req_set(req_addr, PORT_ARQ_REQ_SOURCE, source);
    req_set(req_addr, PORT_ARQ_REQ_DEST, dest);
    req_set(req_addr, PORT_ARQ_REQ_LENGTH, length);
    req_set(req_addr, PORT_ARQ_REQ_CALLBACK, 1); /* always set (hack if NULL) */

    switch (priority) {
    case PORT_ARQ_PRIORITY_LOW:
        if (st->queueLo) {
            req_set(st->tailLo, PORT_ARQ_REQ_NEXT, req_addr);
        } else {
            st->queueLo = req_addr;
        }
        st->tailLo = req_addr;
        break;

    case PORT_ARQ_PRIORITY_HIGH:
        if (st->queueHi) {
            req_set(st->tailHi, PORT_ARQ_REQ_NEXT, req_addr);
        } else {
            st->queueHi = req_addr;
        }
        st->tailHi = req_addr;
        break;
    }

    if ((st->pendingHi == 0) && (st->pendingLo == 0)) {
        port_ARQPopTaskQueueHi(st);

        if (st->pendingHi == 0) {
            port_ARQServiceQueueLo(st);
        }
    }
}
