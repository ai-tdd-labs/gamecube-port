#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

extern uint32_t gc_gx_color_update_enable;
void GXSetColorUpdate(uint8_t enable);

const char *gc_scenario_label(void) { return "GXSetColorUpdate/mp4_main_loop"; }
const char *gc_scenario_out_path(void) { return "../actual/gx_set_color_update_mp4_main_loop_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    GXSetColorUpdate(1);

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 8);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0, 0xDEADBEEFu);
    wr32be(p + 4, gc_gx_color_update_enable);
}
