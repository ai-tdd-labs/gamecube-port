#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

#include "sdk_state.h"

void PADControlMotor(int32_t chan, uint32_t command);

enum {
  PAD_MOTOR_STOP_HARD = 2,
  PAD_MOTOR_RUMBLE = 1,
  PAD_MOTOR_STOP = 0,
};

const char *gc_scenario_label(void) { return "PADControlMotor/mp4_realistic_hupadinit"; }
const char *gc_scenario_out_path(void) { return "../actual/pad_control_motor_realistic_hupadinit_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    PADControlMotor(0, PAD_MOTOR_STOP_HARD);
    PADControlMotor(1, PAD_MOTOR_RUMBLE);
    PADControlMotor(2, PAD_MOTOR_STOP);
    PADControlMotor(3, PAD_MOTOR_STOP_HARD);

    uint32_t c0 = gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_MOTOR_CMD_BASE + 0u);
    uint32_t c1 = gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_MOTOR_CMD_BASE + 4u);
    uint32_t c2 = gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_MOTOR_CMD_BASE + 8u);
    uint32_t c3 = gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_MOTOR_CMD_BASE + 12u);

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x14);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0x00, 0xDEADBEEFu);
    wr32be(p + 0x04, c0);
    wr32be(p + 0x08, c1);
    wr32be(p + 0x0C, c2);
    wr32be(p + 0x10, c3);
}

