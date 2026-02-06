#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

extern uint32_t gc_gx_copy_gamma;

void GXSetDispCopyGamma(uint32_t gamma);

const char *gc_scenario_label(void) { return "GXSetDispCopyGamma/mp4_init_gx"; }
const char *gc_scenario_out_path(void) { return "../actual/gx_set_disp_copy_gamma_mp4_init_gx_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    GXSetDispCopyGamma(0);

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 8);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0x00, 0xDEADBEEFu);
    wr32be(p + 0x04, gc_gx_copy_gamma);
}
