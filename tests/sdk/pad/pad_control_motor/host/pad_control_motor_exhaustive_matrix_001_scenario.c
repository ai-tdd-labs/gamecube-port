#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

#include "sdk_state.h"

void PADControlMotor(int32_t chan, uint32_t command);

static const int32_t k_channels[] = {-2, -1, 0, 1, 2, 3, 4, 5};
static const uint32_t k_cmds[] = {0u, 1u, 2u, 3u, 0xffffffffu};

enum {
    CASE_COUNT = (sizeof(k_channels) / sizeof(k_channels[0])) * (sizeof(k_cmds) / sizeof(k_cmds[0])),
    TG_HDR_WORDS = 4u,
    TG_CASE_WORDS = 8u,
};

const char *gc_scenario_label(void) { return "PADControlMotor/exhaustive_matrix_001"; }
const char *gc_scenario_out_path(void) { return "../actual/pad_control_motor_exhaustive_matrix_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, (TG_HDR_WORDS + CASE_COUNT * TG_CASE_WORDS) * 4u);
    if (!p) die("gc_ram_ptr failed");

    uint32_t word_off = 0u;
    wr32be(p + word_off * 4u, 0x504D5447u); word_off++;
    wr32be(p + word_off * 4u, 0xEEEE0001u); word_off++;
    wr32be(p + word_off * 4u, CASE_COUNT); word_off++;
    wr32be(p + word_off * 4u, 0xE001u); word_off++;

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

            wr32be(p + word_off * 4u, idx); word_off++;
            wr32be(p + word_off * 4u, (uint32_t)chan); word_off++;
            wr32be(p + word_off * 4u, cmd); word_off++;
            wr32be(p + word_off * 4u, cmd); word_off++;
            wr32be(p + word_off * 4u, out0); word_off++;
            wr32be(p + word_off * 4u, out1); word_off++;
            wr32be(p + word_off * 4u, out2); word_off++;
            wr32be(p + word_off * 4u, out3); word_off++;
            idx++;
        }
    }
}
