#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

#include "sdk_state.h"
#include "trace-guided/pad_control_motor/pad_control_motor_trace_guided_gen.h"

void PADControlMotor(int32_t chan, uint32_t command);

enum {
    TG_SEED = 0xC0DE1234u,
    TG_COUNT = 2048u,
    TG_HDR_WORDS = 4u,
    TG_CASE_WORDS = 8u,
};

const char *gc_scenario_label(void) { return "PADControlMotor/trace_guided_batch_002"; }
const char *gc_scenario_out_path(void) { return "../actual/pad_control_motor_trace_guided_batch_002.bin"; }

void gc_scenario_run(GcRam *ram) {
    TgPadMotorRng rng;
    tg_pad_motor_seed(&rng, TG_SEED);

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, (TG_HDR_WORDS + TG_COUNT * TG_CASE_WORDS) * 4u);
    if (!p) die("gc_ram_ptr failed");

    uint32_t word_off = 0u;
    wr32be(p + word_off * 4u, 0x504D5447u); word_off++; // PMTG
    wr32be(p + word_off * 4u, TG_SEED); word_off++;
    wr32be(p + word_off * 4u, TG_COUNT); word_off++;
    wr32be(p + word_off * 4u, 2u); word_off++;

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

        wr32be(p + word_off * 4u, i); word_off++;
        wr32be(p + word_off * 4u, chan); word_off++;
        wr32be(p + word_off * 4u, cmd); word_off++;
        wr32be(p + word_off * 4u, cmd); word_off++;
        wr32be(p + word_off * 4u, out0); word_off++;
        wr32be(p + word_off * 4u, out1); word_off++;
        wr32be(p + word_off * 4u, out2); word_off++;
        wr32be(p + word_off * 4u, out3); word_off++;
    }
}
