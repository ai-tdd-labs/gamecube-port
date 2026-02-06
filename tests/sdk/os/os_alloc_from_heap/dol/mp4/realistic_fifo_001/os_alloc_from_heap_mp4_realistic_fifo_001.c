#include <dolphin/types.h>

#include "../../../../os_init_alloc/dol/oracle_os_init_alloc.h"
#include "../../../../os_create_heap/dol/oracle_os_create_heap.h"
#include "../../../../os_set_current_heap/dol/oracle_os_set_current_heap.h"
#include "../../oracle_os_alloc_from_heap.h"

static void write_result(u32 marker, u32 p, u32 heap, u32 curr) {
    volatile u32 *out = (volatile u32 *)0x80300000;
    out[0] = marker;
    out[1] = p;
    out[2] = heap;
    out[3] = curr;
}

int main(void) {
    void *arena_lo = (void *)0x80004000;
    void *arena_hi = (void *)0x81700000;
    void *new_lo = OSInitAlloc(arena_lo, arena_hi, 1);
    int heap = OSCreateHeap(new_lo, arena_hi);
    OSSetCurrentHeap(heap);

    // MP4 HuSysInit: DefaultFifo = OSAlloc(0x100000)
    void *p = OSAllocFromHeap(__OSCurrHeap, 0x100000);

    write_result(0xDEADBEEFu, (u32)p, (u32)heap, (u32)__OSCurrHeap);
    while (1) {}
}

