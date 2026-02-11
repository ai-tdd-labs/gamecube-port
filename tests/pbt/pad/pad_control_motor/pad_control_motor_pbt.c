#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "../../../../src/sdk_port/gc_mem.h"
#include "../../../../src/sdk_port/sdk_state.h"

typedef int32_t s32;
typedef uint32_t u32;

void PADControlMotor(s32 chan, u32 command);

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

    u32 model[4] = {0, 0, 0, 0};
    for (uint32_t i = 0; i < iters; i++) {
        s32 chan = (s32)(xs32(&seed) % 8u) - 2; // [-2..5]
        u32 cmd = xs32(&seed);

        if (chan >= 0 && chan < 4) {
            model[(u32)chan] = cmd;
        }

        PADControlMotor(chan, cmd);

        for (u32 c = 0; c < 4u; c++) {
            u32 got = gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_MOTOR_CMD_BASE + c * 4u);
            if (got != model[c]) {
                fprintf(stderr, "PBT FAIL step=%u chan=%d slot=%u got=0x%08X exp=0x%08X\n", i, (int)chan, c, got, model[c]);
                return 1;
            }
        }
    }

    printf("PBT PASS: pad_control_motor %u iterations\n", iters);
    return 0;
}
