#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

void VISetNextFrameBuffer(void *fb);

const char *gc_scenario_label(void) { return "VISetNextFrameBuffer/min_001"; }
const char *gc_scenario_out_path(void) { return "../actual/vi_set_next_frame_buffer_min_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    void *xfb = (void *)0x81000000;
    VISetNextFrameBuffer(xfb);

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 8);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0, 0xDEADBEEFu);
    wr32be(p + 4, 0x81000000u);
}

