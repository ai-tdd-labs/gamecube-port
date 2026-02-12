/*
 * sdk_port/ar/ar.h --- Host-side port of GC SDK AR (ARAM allocator).
 *
 * AR is a simple LIFO stack allocator for ARAM.
 * Block lengths stored in gc_mem (big-endian u32 array).
 */
#pragma once

#include <stdint.h>

/* AR state (host-side) */
typedef struct {
    uint32_t stackPointer;     /* current ARAM stack top */
    uint32_t freeBlocks;       /* remaining capacity */
    uint32_t blockLengthBase;  /* GC addr of block length array in gc_mem */
    uint32_t blockLengthIdx;   /* current index into the array */
    uint32_t size;             /* total ARAM size */
    int      initFlag;
} port_ARState;

uint32_t port_ARInit(port_ARState *st, uint32_t blockLengthBase,
                     uint32_t numEntries, uint32_t aramSize);
uint32_t port_ARAlloc(port_ARState *st, uint32_t length);
uint32_t port_ARFree(port_ARState *st, uint32_t *length);
int      port_ARCheckInit(port_ARState *st);
uint32_t port_ARGetBaseAddress(port_ARState *st);
uint32_t port_ARGetSize(port_ARState *st);
