// Oracle snippet copied from:
//   decomp_mario_party_4/src/dolphin/os/OSAlloc.c
//
// Keep this file small and self-contained so DOL tests do not depend on
// linking the full SDK port. The DOL output (expected.bin) is the oracle.

#ifndef ORACLE_OS_SET_CURRENT_HEAP_H
#define ORACLE_OS_SET_CURRENT_HEAP_H

#include <dolphin/types.h>

// NOTE: oracle_os_init_alloc.h defines the storage for __OSCurrHeap in DOL tests.
// Keep this header compatible by using the same type.
extern volatile s32 __OSCurrHeap;

static inline s32 oracle_OSSetCurrentHeap(s32 heap) {
    s32 prev = __OSCurrHeap;
    __OSCurrHeap = heap;
    return prev;
}

// Expose the expected name/signature.
static inline s32 OSSetCurrentHeap(s32 heap) { return oracle_OSSetCurrentHeap(heap); }

#endif
