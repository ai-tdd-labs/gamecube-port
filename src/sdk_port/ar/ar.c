/*
 * sdk_port/ar/ar.c --- Host-side port of GC SDK AR allocator.
 *
 * Source of truth: external/mp4-decomp/src/dolphin/ar/ar.c
 *
 * LIFO stack allocator for ARAM. Block lengths stored in gc_mem as
 * big-endian u32 array. Hardware init (__ARChecksize, DSP regs,
 * interrupt handler) stripped.
 */
#include <stdint.h>
#include "ar.h"
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

/* ── ARInit (ar.c:103-135) ── */

uint32_t port_ARInit(port_ARState *st, uint32_t blockLengthBase,
                     uint32_t numEntries, uint32_t aramSize)
{
    if (st->initFlag) {
        return 0x4000;
    }

    st->stackPointer = 0x4000;
    st->freeBlocks = numEntries;
    st->blockLengthBase = blockLengthBase;
    st->blockLengthIdx = 0;
    st->size = aramSize;
    st->initFlag = 1;

    return st->stackPointer;
}

/* ── ARAlloc (ar.c:61-75) ── */

uint32_t port_ARAlloc(port_ARState *st, uint32_t length)
{
    uint32_t tmp = st->stackPointer;
    st->stackPointer += length;

    /* *__AR_BlockLength = length; __AR_BlockLength++; */
    uint32_t addr = st->blockLengthBase + st->blockLengthIdx * 4;
    store_u32be(addr, length);
    st->blockLengthIdx++;

    st->freeBlocks--;

    return tmp;
}

/* ── ARFree (ar.c:77-96) ── */

uint32_t port_ARFree(port_ARState *st, uint32_t *length)
{
    /* __AR_BlockLength-- */
    st->blockLengthIdx--;

    uint32_t addr = st->blockLengthBase + st->blockLengthIdx * 4;
    uint32_t blockLen = load_u32be(addr);

    if (length) {
        *length = blockLen;
    }

    st->stackPointer -= blockLen;
    st->freeBlocks++;

    return st->stackPointer;
}

/* ── ARCheckInit (ar.c:98-101) ── */

int port_ARCheckInit(port_ARState *st)
{
    return st->initFlag;
}

/* ── ARGetBaseAddress (ar.c:139-142) ── */

uint32_t port_ARGetBaseAddress(port_ARState *st)
{
    (void)st;
    return 0x4000;
}

/* ── ARGetSize (ar.c:144-147) ── */

uint32_t port_ARGetSize(port_ARState *st)
{
    return st->size;
}
