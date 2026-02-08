#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

#include "sdk_state.h"

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

void PADClamp(PADStatus *status);

const char *gc_scenario_label(void) { return "PADClamp/mp4_realistic_padreadvsync"; }
const char *gc_scenario_out_path(void) { return "../actual/pad_clamp_realistic_padreadvsync_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    PADStatus status[4];
    PADClamp(status);

    uint32_t calls = gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_CLAMP_CALLS);

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x08);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0x00, 0xDEADBEEFu);
    wr32be(p + 0x04, calls);
}
