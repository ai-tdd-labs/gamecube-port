#include <stdint.h>

#include "src/sdk_port/gc_mem.c"
#include "src/sdk_port/sdk_state.h"

#include "src/sdk_port/pad/PAD.c"

static inline void store_u32be_ptr(volatile uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)(v >> 0);
}

static const int32_t k_channels[] = {-2, -1, 0, 1, 2, 3, 4, 5};
static const uint32_t k_cmds[] = {0u, 1u, 2u, 3u, 0xffffffffu};

enum {
    CASE_COUNT = (sizeof(k_channels) / sizeof(k_channels[0])) * (sizeof(k_cmds) / sizeof(k_cmds[0])),
    TG_HDR_WORDS = 4u,
    TG_CASE_WORDS = 8u,
};

int main(void) {
    gc_mem_set(0x80000000u, 0x01800000u, (uint8_t *)0x80000000u);
    gc_sdk_state_reset();

    volatile uint8_t *out = (volatile uint8_t *)0x80300000u;
    uint32_t word_off = 0u;

    store_u32be_ptr(out + word_off * 4u, 0x504D5447u); word_off++; // PMTG
    store_u32be_ptr(out + word_off * 4u, 0xEEEE0001u); word_off++;
    store_u32be_ptr(out + word_off * 4u, (uint32_t)CASE_COUNT); word_off++;
    store_u32be_ptr(out + word_off * 4u, 0xE001u); word_off++; // suite id

    uint32_t idx = 0u;
    for (uint32_t ci = 0; ci < sizeof(k_channels) / sizeof(k_channels[0]); ci++) {
        for (uint32_t mi = 0; mi < sizeof(k_cmds) / sizeof(k_cmds[0]); mi++) {
            int32_t chan = k_channels[ci];
            uint32_t cmd = k_cmds[mi];

            for (uint32_t s = 0; s < 4u; s++) {
                gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_MOTOR_CMD_BASE + s * 4u, 0u);
            }

            PADControlMotor(chan, cmd);

            uint32_t out0 = gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_MOTOR_CMD_BASE + 0u);
            uint32_t out1 = gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_MOTOR_CMD_BASE + 4u);
            uint32_t out2 = gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_MOTOR_CMD_BASE + 8u);
            uint32_t out3 = gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_MOTOR_CMD_BASE + 12u);

            store_u32be_ptr(out + word_off * 4u, idx); word_off++;
            store_u32be_ptr(out + word_off * 4u, (uint32_t)chan); word_off++;
            store_u32be_ptr(out + word_off * 4u, cmd); word_off++;
            store_u32be_ptr(out + word_off * 4u, cmd); word_off++;
            store_u32be_ptr(out + word_off * 4u, out0); word_off++;
            store_u32be_ptr(out + word_off * 4u, out1); word_off++;
            store_u32be_ptr(out + word_off * 4u, out2); word_off++;
            store_u32be_ptr(out + word_off * 4u, out3); word_off++;
            idx++;
        }
    }

    while (1) {
        __asm__ volatile("nop");
    }
    return 0;
}
