#include <stdint.h>

#include "src/sdk_port/sdk_state.h"

typedef uint32_t u32;
typedef int BOOL;

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

BOOL PADRecalibrate(u32 mask) {
    gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_RECALIBRATE_MASK, mask);
    gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_RECALIBRATE_CALLS,
                             gc_sdk_state_load_u32_or(GC_SDK_OFF_PAD_RECALIBRATE_CALLS, 0) + 1u);
    gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_RECALIBRATE_BITS, mask);
    return TRUE;
}
