#include <dolphin/types.h>

#include "../../../../os_init_alloc/dol/oracle_os_init_alloc.h"
#include "../../../../os_create_heap/dol/oracle_os_create_heap.h"
#include "../../../../os_set_current_heap/dol/oracle_os_set_current_heap.h"
#include "../../oracle_os_alloc_from_heap.h"

static void write_result(u32 marker, u32 p1, u32 p2) {
    volatile u32 *out = (volatile u32 *)0x80300000;
    out[0] = marker;
    out[1] = p1;
    out[2] = p2;
}

int main(void) {
    OSInitAlloc((void *)0x80002000, (void *)0x80008000, 2);
    int heap = OSCreateHeap((void *)0x80003000, (void *)0x80007000);
    OSSetCurrentHeap(heap);

    void *p1 = OSAllocFromHeap(__OSCurrHeap, 0x20);
    void *p2 = OSAllocFromHeap(__OSCurrHeap, 0x40);

    write_result(0xDEADBEEFu, (u32)p1, (u32)p2);
    while (1) {}
}

