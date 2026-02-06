#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

extern uint32_t gc_gx_pixel_fmt;
extern uint32_t gc_gx_z_fmt;

void GXSetPixelFmt(uint32_t pix_fmt, uint32_t z_fmt);

const char *gc_scenario_label(void) { return "GXSetPixelFmt/mp4_init_gx"; }
const char *gc_scenario_out_path(void) { return "../actual/gx_set_pixel_fmt_mp4_init_gx_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    GXSetPixelFmt(2, 1);

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x0C);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0x00, 0xDEADBEEFu);
    wr32be(p + 0x04, gc_gx_pixel_fmt);
    wr32be(p + 0x08, gc_gx_z_fmt);
}
