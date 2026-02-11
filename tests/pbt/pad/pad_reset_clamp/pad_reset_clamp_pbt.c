#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "../../../../src/sdk_port/gc_mem.h"
#include "../../../../src/sdk_port/sdk_state.h"

typedef uint32_t u32;
typedef int BOOL;
typedef struct PADStatus {
    uint16_t button;
    int8_t stickX;
    int8_t stickY;
    int8_t substickX;
    int8_t substickY;
    uint8_t triggerL;
    uint8_t triggerR;
    uint8_t analogA;
    uint8_t analogB;
    int8_t err;
} PADStatus;

BOOL PADReset(u32 mask);
void PADClamp(PADStatus *status);

static uint32_t xs32(uint32_t *state) {
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

int main(int argc, char **argv) {
    uint32_t iters = 200000;
    uint32_t seed = 0xC0DEC0DEu;
    if (argc >= 2) iters = (uint32_t)strtoul(argv[1], 0, 0);
    if (argc >= 3) seed = (uint32_t)strtoul(argv[2], 0, 0);

    static uint8_t mem1[0x01800000];
    gc_mem_set(0x80000000u, sizeof(mem1), mem1);
    gc_sdk_state_reset();

    for (uint32_t i = 0; i < iters; i++) {
        u32 pre_resetting_bits = xs32(&seed);
        u32 pre_recal_bits = xs32(&seed);
        u32 pre_cb = xs32(&seed);
        u32 mask = xs32(&seed);

        // Seed pre-state exactly as PADReset consumes it.
        gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_RESETTING_BITS, pre_resetting_bits);
        gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_RECALIBRATE_BITS, pre_recal_bits);
        gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_RESET_CB_PTR, pre_cb);
        gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_RESETTING_CHAN, 32u);

        u32 reset_calls_before = gc_sdk_state_load_u32_or(GC_SDK_OFF_PAD_RESET_CALLS, 0);
        BOOL ok = PADReset(mask);
        if (!ok) {
            fprintf(stderr, "PBT FAIL step=%u: PADReset returned FALSE\n", i);
            return 1;
        }

        u32 union_bits = pre_resetting_bits | mask;
        u32 exp_chan = (union_bits == 0u) ? 32u : (u32)__builtin_clz(union_bits);
        u32 exp_bits = union_bits;
        if (exp_chan != 32u) {
            exp_bits &= ~(0x80000000u >> exp_chan);
        }

        u32 got_mask = gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_RESET_MASK);
        u32 got_calls = gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_RESET_CALLS);
        u32 got_bits = gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_RESETTING_BITS);
        u32 got_chan = gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_RESETTING_CHAN);
        u32 got_recal_bits = gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_RECALIBRATE_BITS);
        u32 got_cb = gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_RESET_CB_PTR);

        if (got_mask != mask || got_calls != reset_calls_before + 1u) {
            fprintf(stderr, "PBT FAIL step=%u: reset metadata mismatch\n", i);
            return 1;
        }
        if (got_bits != exp_bits || got_chan != exp_chan) {
            fprintf(stderr, "PBT FAIL step=%u: reset core mismatch bits got=%08X exp=%08X chan got=%u exp=%u\n",
                    i, got_bits, exp_bits, got_chan, exp_chan);
            return 1;
        }
        if (got_recal_bits != pre_recal_bits || got_cb != pre_cb) {
            fprintf(stderr, "PBT FAIL step=%u: unrelated state changed\n", i);
            return 1;
        }

        // PADClamp only increments call counter in current model.
        PADStatus st[4] = {0};
        u32 clamp_before = gc_sdk_state_load_u32_or(GC_SDK_OFF_PAD_CLAMP_CALLS, 0);
        PADClamp(st);
        u32 clamp_after = gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_CLAMP_CALLS);
        if (clamp_after != clamp_before + 1u) {
            fprintf(stderr, "PBT FAIL step=%u: PADClamp call counter mismatch\n", i);
            return 1;
        }
    }

    printf("PBT PASS: pad_reset_clamp %u iterations\n", iters);
    return 0;
}
