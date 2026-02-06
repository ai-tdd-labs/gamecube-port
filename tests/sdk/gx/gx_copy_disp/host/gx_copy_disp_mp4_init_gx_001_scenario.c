#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

extern uint32_t gc_gx_copy_disp_dest;
extern uint32_t gc_gx_copy_disp_clear;

void GXCopyDisp(void *dest, uint8_t clear);

const char *gc_scenario_label(void) { return "GXCopyDisp/mp4_init_gx"; }
const char *gc_scenario_out_path(void) { return "../actual/gx_copy_disp_mp4_init_gx_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    GXCopyDisp((void*)0x81234000u, 1);

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x0C);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0x00, 0xDEADBEEFu);
    wr32be(p + 0x04, gc_gx_copy_disp_dest);
    wr32be(p + 0x08, gc_gx_copy_disp_clear);
}
