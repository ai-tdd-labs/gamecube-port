#include <stdint.h>

#include "src/sdk_port/sdk_state.h"

typedef int BOOL;
typedef int32_t s32;
typedef uint32_t u32;

#ifndef TRUE
#define TRUE 1
#endif

#define PAD_CHAN0_BIT 0x80000000u

static u32 RecalibrateBits;
static u32 ResettingBits;
static s32 ResettingChan = 32;
static u32 ResetCallbackPtr;

BOOL PADReset(u32 mask) {
    ResettingBits = gc_sdk_state_load_u32_or(GC_SDK_OFF_PAD_RESETTING_BITS, ResettingBits);
    ResettingChan = (s32)gc_sdk_state_load_u32_or(GC_SDK_OFF_PAD_RESETTING_CHAN, (u32)ResettingChan);
    RecalibrateBits = gc_sdk_state_load_u32_or(GC_SDK_OFF_PAD_RECALIBRATE_BITS, RecalibrateBits);
    ResetCallbackPtr = gc_sdk_state_load_u32_or(GC_SDK_OFF_PAD_RESET_CB_PTR, ResetCallbackPtr);

    gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_RESET_MASK, mask);
    gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_RESET_CALLS,
                             gc_sdk_state_load_u32_or(GC_SDK_OFF_PAD_RESET_CALLS, 0) + 1u);

    ResettingBits |= mask;
    ResettingChan = (ResettingBits == 0u) ? 32 : (s32)__builtin_clz(ResettingBits);
    if (ResettingChan != 32) {
        u32 chanBit = PAD_CHAN0_BIT >> (u32)ResettingChan;
        ResettingBits &= ~chanBit;
    }
    gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_RESETTING_BITS, ResettingBits);
    gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_RESETTING_CHAN, (u32)ResettingChan);
    gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_RECALIBRATE_BITS, RecalibrateBits);
    gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_RESET_CB_PTR, ResetCallbackPtr);
    return TRUE;
}
