#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

#include "sdk_state.h"

int PADRecalibrate(uint32_t mask);

const char *gc_scenario_label(void) { return "PADRecalibrate/mp4_realistic_padreadvsync"; }
const char *gc_scenario_out_path(void) { return "../actual/pad_recalibrate_realistic_padreadvsync_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    uint32_t mask = 0x60000000u;
    uint32_t ok = (uint32_t)PADRecalibrate(mask);

    uint32_t calls = gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_RECALIBRATE_CALLS);
    uint32_t got_mask = gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_RECALIBRATE_MASK);

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x10);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0x00, 0xDEADBEEFu);
    wr32be(p + 0x04, ok);
    wr32be(p + 0x08, calls);
    wr32be(p + 0x0C, got_mask);
}
