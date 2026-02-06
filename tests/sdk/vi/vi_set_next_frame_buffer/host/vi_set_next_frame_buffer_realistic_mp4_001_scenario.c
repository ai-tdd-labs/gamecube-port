#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

void VISetNextFrameBuffer(void *fb);

const char *gc_scenario_label(void) { return "VISetNextFrameBuffer/realistic_mp4_001"; }
const char *gc_scenario_out_path(void) { return "../actual/vi_set_next_frame_buffer_realistic_mp4_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    void *xfb1 = (void *)0x81000000;
    void *xfb2 = (void *)0x81002000;
    VISetNextFrameBuffer(xfb1);
    VISetNextFrameBuffer(xfb2);

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 8);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0, 0xDEADBEEFu);
    wr32be(p + 4, 0x81002000u);
}

