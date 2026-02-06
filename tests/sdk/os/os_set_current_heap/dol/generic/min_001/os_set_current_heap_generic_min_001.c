#include <dolphin/types.h>

// Oracle snippets copied from MP4 SDK source.
#include "../../../../os_init_alloc/dol/oracle_os_init_alloc.h"
#include "../../../../os_create_heap/dol/oracle_os_create_heap.h"
#include "../../oracle_os_set_current_heap.h"

static void write_result(u32 marker, u32 prev, u32 curr) {
    volatile u32 *out = (volatile u32 *)0x80300000;
    out[0] = marker;
    out[1] = prev;
    out[2] = curr;
}

int main(void) {
    void *arena_lo = (void *)0x80002000;
    void *arena_hi = (void *)0x80006000;

    OSInitAlloc(arena_lo, arena_hi, 2);

    int heap = OSCreateHeap((void *)0x80003000, (void *)0x80005000);
    int prev = OSSetCurrentHeap(heap);

    write_result(0xDEADBEEFu, (u32)prev, (u32)__OSCurrHeap);
    while (1) {}
}
