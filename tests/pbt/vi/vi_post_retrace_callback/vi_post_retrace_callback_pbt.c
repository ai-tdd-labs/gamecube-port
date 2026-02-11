#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "../../../../src/sdk_port/gc_mem.h"
#include "../../../../src/sdk_port/sdk_state.h"

typedef uint32_t u32;
typedef void (*VIRetraceCallback)(u32 retraceCount);

VIRetraceCallback VISetPostRetraceCallback(VIRetraceCallback callback);
void VIWaitForRetrace(void);

static uint32_t xs32(uint32_t *state) {
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

static u32 g_cb1_calls;
static u32 g_cb2_calls;
static u32 g_cb1_last;
static u32 g_cb2_last;
static void cb1(u32 rc) {
    g_cb1_calls++;
    g_cb1_last = rc;
}
static void cb2(u32 rc) {
    g_cb2_calls++;
    g_cb2_last = rc;
}

int main(int argc, char **argv) {
    uint32_t iters = 200000;
    uint32_t seed = 0xC0DEC0DEu;
    if (argc >= 2) iters = (uint32_t)strtoul(argv[1], 0, 0);
    if (argc >= 3) seed = (uint32_t)strtoul(argv[2], 0, 0);

    static uint8_t mem1[0x01800000];
    gc_mem_set(0x80000000u, sizeof(mem1), mem1);
    gc_sdk_state_reset();

    VIRetraceCallback model_cb = 0;
    u32 model_set_calls = 0;

    for (uint32_t i = 0; i < iters; i++) {
        uint32_t pick = xs32(&seed) % 3u;
        VIRetraceCallback next = (pick == 0u) ? 0 : ((pick == 1u) ? cb1 : cb2);

        u32 disable_before = gc_sdk_state_load_u32_or(GC_SDK_OFF_VI_DISABLE_CALLS, 0);
        u32 restore_before = gc_sdk_state_load_u32_or(GC_SDK_OFF_VI_RESTORE_CALLS, 0);
        u32 set_calls_before = gc_sdk_state_load_u32_or(GC_SDK_OFF_VI_POST_CB_SET_CALLS, 0);

        VIRetraceCallback old = VISetPostRetraceCallback(next);
        if (old != model_cb) {
            fprintf(stderr, "PBT FAIL step=%u: return old callback mismatch\n", i);
            return 1;
        }
        model_cb = next;
        model_set_calls++;

        u32 disable_after = gc_sdk_state_load_u32_or(GC_SDK_OFF_VI_DISABLE_CALLS, 0);
        u32 restore_after = gc_sdk_state_load_u32_or(GC_SDK_OFF_VI_RESTORE_CALLS, 0);
        u32 set_calls_after = gc_sdk_state_load_u32_or(GC_SDK_OFF_VI_POST_CB_SET_CALLS, 0);
        u32 ptr_token = gc_sdk_state_load_u32_or(GC_SDK_OFF_VI_POST_CB_PTR, 0);

        if (disable_after != disable_before + 1u || restore_after != restore_before + 1u) {
            fprintf(stderr, "PBT FAIL step=%u: interrupt wrap mismatch\n", i);
            return 1;
        }
        if (set_calls_after != set_calls_before + 1u || set_calls_after != model_set_calls) {
            fprintf(stderr, "PBT FAIL step=%u: set-calls mismatch\n", i);
            return 1;
        }
        if (next == 0 && ptr_token != 0u) {
            fprintf(stderr, "PBT FAIL step=%u: NULL callback must store token 0\n", i);
            return 1;
        }
        if (next != 0 && ptr_token == 0u) {
            fprintf(stderr, "PBT FAIL step=%u: non-NULL callback must store non-zero token\n", i);
            return 1;
        }

        // Validate callback dispatch side effects.
        u32 post_calls_before = gc_sdk_state_load_u32_or(GC_SDK_OFF_VI_POST_CB_CALLS, 0);
        u32 retrace_before = gc_sdk_state_load_u32_or(GC_SDK_OFF_VI_RETRACE_COUNT, 0);
        VIWaitForRetrace();
        u32 retrace_after = gc_sdk_state_load_u32_or(GC_SDK_OFF_VI_RETRACE_COUNT, 0);
        u32 post_calls_after = gc_sdk_state_load_u32_or(GC_SDK_OFF_VI_POST_CB_CALLS, 0);
        u32 post_last_arg = gc_sdk_state_load_u32_or(GC_SDK_OFF_VI_POST_CB_LAST_ARG, 0);
        if (retrace_after != retrace_before + 1u) {
            fprintf(stderr, "PBT FAIL step=%u: retrace counter mismatch\n", i);
            return 1;
        }
        if (next) {
            if (post_calls_after != post_calls_before + 1u || post_last_arg != retrace_after) {
                fprintf(stderr, "PBT FAIL step=%u: callback call-side effects mismatch\n", i);
                return 1;
            }
        } else {
            if (post_calls_after != post_calls_before) {
                fprintf(stderr, "PBT FAIL step=%u: callback calls changed while NULL\n", i);
                return 1;
            }
        }
    }

    // Make sure real callbacks were actually exercised.
    if (g_cb1_calls == 0 || g_cb2_calls == 0) {
        fprintf(stderr, "PBT FAIL: callbacks not exercised\n");
        return 1;
    }
    (void)g_cb1_last;
    (void)g_cb2_last;

    printf("PBT PASS: vi_post_retrace_callback %u iterations\n", iters);
    return 0;
}
