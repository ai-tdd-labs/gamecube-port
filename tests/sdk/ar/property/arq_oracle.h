/*
 * arq_oracle.h --- Oracle for ARQ (ARAM Request Queue) property tests.
 *
 * Exact copy of decomp ARQPostRequest, __ARQPopTaskQueueHi,
 * __ARQServiceQueueLo, __ARQInterruptServiceRoutine with oracle_ prefix.
 * Hardware calls (ARStartDMA, OSDisableInterrupts) replaced with mocks.
 *
 * Source of truth: external/mp4-decomp/src/dolphin/ar/arq.c
 *                  external/mp4-decomp/include/dolphin/arq.h
 */
#pragma once

#include <stdint.h>
#include <string.h>

/* Oracle strictness tier for reporting/gating. */
#define ORACLE_TIER "DECOMP_ADAPTED"

/* ── Constants (from arq.h / ar.h) ── */
#define ORACLE_ARQ_TYPE_MRAM_TO_ARAM  0
#define ORACLE_ARQ_TYPE_ARAM_TO_MRAM  1
#define ORACLE_ARQ_PRIORITY_LOW       0
#define ORACLE_ARQ_PRIORITY_HIGH      1
#define ORACLE_ARQ_CHUNK_SIZE_DEFAULT 4096

/* ── ARQRequest struct (from arq.h, 32 bytes on GC) ── */
typedef struct oracle_ARQRequest {
    struct oracle_ARQRequest *next;
    uint32_t owner;
    uint32_t type;
    uint32_t priority;
    uint32_t source;
    uint32_t dest;
    uint32_t length;
    int      has_callback;  /* 1 = real callback, 0 = hack/noop */
} oracle_ARQRequest;

/* ── DMA record for comparison ── */
typedef struct {
    uint32_t type;
    uint32_t mainmem;
    uint32_t aram;
    uint32_t length;
} oracle_DMARecord;

#define ORACLE_MAX_DMA 2048

/* ── Oracle global state ── */
static oracle_ARQRequest *oracle_QueueHi;
static oracle_ARQRequest *oracle_TailHi;
static oracle_ARQRequest *oracle_QueueLo;
static oracle_ARQRequest *oracle_TailLo;
static oracle_ARQRequest *oracle_PendingHi;
static oracle_ARQRequest *oracle_PendingLo;
static int                oracle_HasCallbackHi;
static int                oracle_HasCallbackLo;
static uint32_t           oracle_ChunkSize;

/* DMA log */
static oracle_DMARecord oracle_dma_log[ORACLE_MAX_DMA];
static int              oracle_dma_count;

/* Callback log: record which request got its callback fired */
static int oracle_callback_count;

/* ── Init ── */
static void oracle_ARQInit(void)
{
    oracle_QueueHi = oracle_TailHi = NULL;
    oracle_QueueLo = oracle_TailLo = NULL;
    oracle_PendingHi = oracle_PendingLo = NULL;
    oracle_HasCallbackHi = 0;
    oracle_HasCallbackLo = 0;
    oracle_ChunkSize = ORACLE_ARQ_CHUNK_SIZE_DEFAULT;
    oracle_dma_count = 0;
    oracle_callback_count = 0;
}

/* ── Mock ARStartDMA: just log the DMA parameters ── */
static void oracle_ARStartDMA(uint32_t type, uint32_t mainmem,
                               uint32_t aram, uint32_t length)
{
    if (oracle_dma_count < ORACLE_MAX_DMA) {
        oracle_dma_log[oracle_dma_count].type    = type;
        oracle_dma_log[oracle_dma_count].mainmem = mainmem;
        oracle_dma_log[oracle_dma_count].aram    = aram;
        oracle_dma_log[oracle_dma_count].length  = length;
        oracle_dma_count++;
    }
}

/* ────────────────────────────────────────────────────────────────────
 * __ARQPopTaskQueueHi  (arq.c:23-40, exact logic)
 * ──────────────────────────────────────────────────────────────────── */
static void oracle_ARQPopTaskQueueHi(void)
{
    if (oracle_QueueHi) {
        if (oracle_QueueHi->type == ORACLE_ARQ_TYPE_MRAM_TO_ARAM) {
            oracle_ARStartDMA(oracle_QueueHi->type,
                              oracle_QueueHi->source,
                              oracle_QueueHi->dest,
                              oracle_QueueHi->length);
        } else {
            oracle_ARStartDMA(oracle_QueueHi->type,
                              oracle_QueueHi->dest,
                              oracle_QueueHi->source,
                              oracle_QueueHi->length);
        }

        oracle_HasCallbackHi = 1; /* callback always set (hack or real) */
        oracle_PendingHi = oracle_QueueHi;
        oracle_QueueHi = oracle_QueueHi->next;
    }
}

/* ────────────────────────────────────────────────────────────────────
 * __ARQServiceQueueLo  (arq.c:42-73, exact logic)
 * ──────────────────────────────────────────────────────────────────── */
static void oracle_ARQServiceQueueLo(void)
{
    if ((oracle_PendingLo == NULL) && (oracle_QueueLo)) {
        oracle_PendingLo = oracle_QueueLo;
        oracle_QueueLo = oracle_QueueLo->next;
    }

    if (oracle_PendingLo) {
        if (oracle_PendingLo->length <= oracle_ChunkSize) {
            if (oracle_PendingLo->type == ORACLE_ARQ_TYPE_MRAM_TO_ARAM)
                oracle_ARStartDMA(oracle_PendingLo->type,
                                  oracle_PendingLo->source,
                                  oracle_PendingLo->dest,
                                  oracle_PendingLo->length);
            else
                oracle_ARStartDMA(oracle_PendingLo->type,
                                  oracle_PendingLo->dest,
                                  oracle_PendingLo->source,
                                  oracle_PendingLo->length);

            oracle_HasCallbackLo = 1; /* last chunk: set callback */
        } else {
            if (oracle_PendingLo->type == ORACLE_ARQ_TYPE_MRAM_TO_ARAM)
                oracle_ARStartDMA(oracle_PendingLo->type,
                                  oracle_PendingLo->source,
                                  oracle_PendingLo->dest,
                                  oracle_ChunkSize);
            else
                oracle_ARStartDMA(oracle_PendingLo->type,
                                  oracle_PendingLo->dest,
                                  oracle_PendingLo->source,
                                  oracle_ChunkSize);
        }

        oracle_PendingLo->length -= oracle_ChunkSize;
        oracle_PendingLo->source += oracle_ChunkSize;
        oracle_PendingLo->dest   += oracle_ChunkSize;
    }
}

/* ────────────────────────────────────────────────────────────────────
 * __ARQInterruptServiceRoutine  (arq.c:76-94, exact logic)
 * ──────────────────────────────────────────────────────────────────── */
static void oracle_ARQInterruptServiceRoutine(void)
{
    if (oracle_HasCallbackHi) {
        /* (*callback)((u32)pendingHi) -- we just count it */
        oracle_callback_count++;
        oracle_PendingHi = NULL;
        oracle_HasCallbackHi = 0;
    }
    else if (oracle_HasCallbackLo) {
        oracle_callback_count++;
        oracle_PendingLo = NULL;
        oracle_HasCallbackLo = 0;
    }

    oracle_ARQPopTaskQueueHi();

    if (oracle_PendingHi == NULL)
        oracle_ARQServiceQueueLo();
}

/* ────────────────────────────────────────────────────────────────────
 * ARQPostRequest  (arq.c:112-166, exact logic)
 *
 * Simplified: no interrupt disable/restore, no callback function
 * pointer (just has_callback flag).
 * ──────────────────────────────────────────────────────────────────── */
static void oracle_ARQPostRequest(oracle_ARQRequest *request,
                                   uint32_t owner, uint32_t type,
                                   uint32_t priority,
                                   uint32_t source, uint32_t dest,
                                   uint32_t length, int has_callback)
{
    request->next     = NULL;
    request->owner    = owner;
    request->type     = type;
    request->source   = source;
    request->dest     = dest;
    request->length   = length;
    request->has_callback = 1; /* decomp always sets callback (hack if NULL) */

    switch (priority) {
    case ORACLE_ARQ_PRIORITY_LOW:
        if (oracle_QueueLo) {
            oracle_TailLo->next = request;
        } else {
            oracle_QueueLo = request;
        }
        oracle_TailLo = request;
        break;

    case ORACLE_ARQ_PRIORITY_HIGH:
        if (oracle_QueueHi) {
            oracle_TailHi->next = request;
        } else {
            oracle_QueueHi = request;
        }
        oracle_TailHi = request;
        break;
    }

    if ((oracle_PendingHi == NULL) && (oracle_PendingLo == NULL)) {
        oracle_ARQPopTaskQueueHi();

        if (oracle_PendingHi == NULL) {
            oracle_ARQServiceQueueLo();
        }
    }
}
