#include <stdint.h>

#include "src/sdk_port/gc_mem.c"
#include "src/sdk_port/sdk_state.h"

#include "src/sdk_port/pad/PAD.c"
#include "tests/trace-guided/pad_control_motor/pad_control_motor_trace_guided_gen.h"

static inline void store_u32be_ptr(volatile uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)(v >> 0);
}

enum {
    TG_SEED = 0xC0DE1234u,
    TG_COUNT = 2048u,
    TG_HDR_WORDS = 4u,
    TG_CASE_WORDS = 8u,
};

int main(void) {
    gc_mem_set(0x80000000u, 0x01800000u, (uint8_t *)0x80000000u);
    gc_sdk_state_reset();

    TgPadMotorRng rng;
    tg_pad_motor_seed(&rng, TG_SEED);

    volatile uint8_t *out = (volatile uint8_t *)0x80300000u;
    uint32_t word_off = 0u;

    store_u32be_ptr(out + word_off * 4u, 0x504D5447u); word_off++; // PMTG
    store_u32be_ptr(out + word_off * 4u, TG_SEED); word_off++;
    store_u32be_ptr(out + word_off * 4u, TG_COUNT); word_off++;
    store_u32be_ptr(out + word_off * 4u, 2u); word_off++; // suite id

    for (uint32_t i = 0; i < TG_COUNT; i++) {
        uint32_t chan = 0u, cmd = 0u;
        tg_pad_motor_pick_case(&rng, &chan, &cmd);

        for (uint32_t s = 0; s < 4u; s++) {
            gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_MOTOR_CMD_BASE + s * 4u, 0u);
        }

        PADControlMotor((int32_t)chan, cmd);

        uint32_t out0 = gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_MOTOR_CMD_BASE + 0u);
        uint32_t out1 = gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_MOTOR_CMD_BASE + 4u);
        uint32_t out2 = gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_MOTOR_CMD_BASE + 8u);
        uint32_t out3 = gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_MOTOR_CMD_BASE + 12u);

        store_u32be_ptr(out + word_off * 4u, i); word_off++;
        store_u32be_ptr(out + word_off * 4u, chan); word_off++;
        store_u32be_ptr(out + word_off * 4u, cmd); word_off++;
        store_u32be_ptr(out + word_off * 4u, cmd); word_off++; // expected_motor_cmd
        store_u32be_ptr(out + word_off * 4u, out0); word_off++;
        store_u32be_ptr(out + word_off * 4u, out1); word_off++;
        store_u32be_ptr(out + word_off * 4u, out2); word_off++;
        store_u32be_ptr(out + word_off * 4u, out3); word_off++;
    }

    while (1) {
        __asm__ volatile("nop");
    }
    return 0;
}
