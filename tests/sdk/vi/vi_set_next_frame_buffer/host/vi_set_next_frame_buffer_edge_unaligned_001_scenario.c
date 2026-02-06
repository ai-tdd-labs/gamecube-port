#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

void VISetNextFrameBuffer(void *fb);

const char *gc_scenario_label(void) { return "VISetNextFrameBuffer/edge_unaligned_001"; }
const char *gc_scenario_out_path(void) { return "../actual/vi_set_next_frame_buffer_edge_unaligned_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 8);
    if (!p) die("gc_ram_ptr failed");

    // Pre-marker: detect early crash/assert vs completed call.
    wr32be(p + 0, 0x11111111u);

    void *xfb = (void *)0x81000004;
    VISetNextFrameBuffer(xfb);

    wr32be(p + 0, 0x22222222u);
    wr32be(p + 4, 0x81000004u);
}

