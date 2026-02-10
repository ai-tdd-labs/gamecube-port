#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

#include "sdk_state.h"

void PADControlMotor(int32_t chan, uint32_t command);

static uint32_t must_u32(const char *key) {
    const char *s = getenv(key);
    if (!s || !*s) die("missing env var");
    char *endp = 0;
    uint32_t v = (uint32_t)strtoul(s, &endp, 0);
    if (!endp || *endp != 0) die("invalid env var");
    return v;
}

const char *gc_scenario_label(void) { return "PADControlMotor/rvz_trace_replay_001"; }

const char *gc_scenario_out_path(void) {
    const char *case_id = getenv("GC_TRACE_CASE_ID");
    static char path[512];
    if (case_id && *case_id) {
        char safe[256];
        size_t j = 0;
        for (size_t i = 0; case_id[i] && j + 1 < sizeof(safe); i++) {
            char c = case_id[i];
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                c == '_' || c == '-' || c == '.') {
                safe[j++] = c;
            }
        }
        safe[j] = 0;
        snprintf(path, sizeof(path), "../actual/pad_control_motor_rvz_trace_%s.bin", safe);
    } else {
        snprintf(path, sizeof(path), "../actual/pad_control_motor_rvz_trace_replay_001.bin");
    }
    return path;
}

void gc_scenario_run(GcRam *ram) {
    uint32_t chan = must_u32("GC_SEED_CHAN");
    uint32_t cmd = must_u32("GC_SEED_CMD");
    uint32_t expected_motor_cmd = must_u32("GC_EXPECTED_MOTOR_CMD");

    // Deterministic: clear the motor slots (4 * u32).
    for (uint32_t i = 0; i < 4; i++) {
        gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_MOTOR_CMD_BASE + i * 4u, 0u);
    }

    PADControlMotor((int32_t)chan, cmd);

    uint32_t out0 = gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_MOTOR_CMD_BASE + 0u);
    uint32_t out1 = gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_MOTOR_CMD_BASE + 4u);
    uint32_t out2 = gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_MOTOR_CMD_BASE + 8u);
    uint32_t out3 = gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_MOTOR_CMD_BASE + 12u);

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x30);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0x00, 0xDEADBEEFu);
    wr32be(p + 0x04, chan);
    wr32be(p + 0x08, cmd);
    wr32be(p + 0x0C, expected_motor_cmd);
    wr32be(p + 0x10, out0);
    wr32be(p + 0x14, out1);
    wr32be(p + 0x18, out2);
    wr32be(p + 0x1C, out3);
}

