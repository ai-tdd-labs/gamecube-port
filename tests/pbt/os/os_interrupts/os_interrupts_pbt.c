#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "../../../../src/sdk_port/gc_mem.h"
#include "../../../../src/sdk_port/sdk_state.h"

int OSDisableInterrupts(void);
int OSEnableInterrupts(void);
int OSRestoreInterrupts(int level);

static uint32_t xs32(uint32_t *state) {
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

static void fail_step(uint32_t i, const char *msg, uint32_t got, uint32_t exp) {
    fprintf(stderr, "PBT FAIL step=%u: %s got=%u exp=%u\n", i, msg, got, exp);
    exit(1);
}

int main(int argc, char **argv) {
    uint32_t iters = 200000;
    uint32_t seed = 0xC0DEC0DEu;
    if (argc >= 2) iters = (uint32_t)strtoul(argv[1], 0, 0);
    if (argc >= 3) seed = (uint32_t)strtoul(argv[2], 0, 0);

    static uint8_t mem1[0x01800000];
    gc_mem_set(0x80000000u, sizeof(mem1), mem1);
    gc_sdk_state_reset();
    gc_sdk_state_store_u32be(GC_SDK_OFF_OS_INTS_ENABLED, 1);

    uint32_t model_enabled = 1;
    uint32_t model_disable_calls = 0;
    uint32_t model_restore_calls = 0;

    for (uint32_t i = 0; i < iters; i++) {
        uint32_t op = xs32(&seed) % 3u;
        uint32_t prev = model_enabled;

        if (op == 0u) {
            int ret = OSDisableInterrupts();
            model_disable_calls++;
            model_enabled = 0;
            if ((uint32_t)ret != prev) fail_step(i, "OSDisableInterrupts return", (uint32_t)ret, prev);
        } else if (op == 1u) {
            int ret = OSEnableInterrupts();
            model_enabled = 1;
            if ((uint32_t)ret != prev) fail_step(i, "OSEnableInterrupts return", (uint32_t)ret, prev);
        } else {
            int level = (xs32(&seed) & 1u) ? 1 : 0;
            int ret = OSRestoreInterrupts(level);
            model_restore_calls++;
            model_enabled = (level != 0) ? 1u : 0u;
            if ((uint32_t)ret != prev) fail_step(i, "OSRestoreInterrupts return", (uint32_t)ret, prev);
        }

        uint32_t got_enabled = gc_sdk_state_load_u32be(GC_SDK_OFF_OS_INTS_ENABLED);
        uint32_t got_disable_calls = gc_sdk_state_load_u32be(GC_SDK_OFF_OS_DISABLE_CALLS);
        uint32_t got_restore_calls = gc_sdk_state_load_u32be(GC_SDK_OFF_OS_RESTORE_CALLS);

        if (got_enabled != model_enabled) fail_step(i, "ints_enabled", got_enabled, model_enabled);
        if (got_disable_calls != model_disable_calls) fail_step(i, "disable_calls", got_disable_calls, model_disable_calls);
        if (got_restore_calls != model_restore_calls) fail_step(i, "restore_calls", got_restore_calls, model_restore_calls);
    }

    printf("PBT PASS: os_interrupts %u iterations\n", iters);
    return 0;
}
