#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

extern uint32_t gc_gx_zmode_enable;
extern uint32_t gc_gx_zmode_func;
extern uint32_t gc_gx_zmode_update_enable;

void GXSetZMode(uint8_t enable, uint32_t func, uint8_t update_enable);

const char *gc_scenario_label(void) { return "GXSetZMode/mp4_main_loop"; }
const char *gc_scenario_out_path(void) { return "../actual/gx_set_z_mode_mp4_main_loop_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    GXSetZMode(1, 3, 1);

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x10);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0x00, 0xDEADBEEFu);
    wr32be(p + 0x04, gc_gx_zmode_enable);
    wr32be(p + 0x08, gc_gx_zmode_func);
    wr32be(p + 0x0C, gc_gx_zmode_update_enable);
}
