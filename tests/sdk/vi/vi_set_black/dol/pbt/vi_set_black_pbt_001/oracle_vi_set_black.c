#include <stdint.h>

#include "src/sdk_port/sdk_state.h"

typedef uint32_t u32;

void VISetBlack(u32 black) {
    u32 c = gc_sdk_state_load_u32_or(GC_SDK_OFF_VI_SET_BLACK_CALLS, 0u);
    c++;
    gc_sdk_state_store_u32be(GC_SDK_OFF_VI_SET_BLACK_CALLS, c);
    gc_sdk_state_store_u32be(GC_SDK_OFF_VI_BLACK, black ? 1u : 0u);
}
