#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

typedef uint32_t u32;
typedef uint8_t u8;

typedef struct {
    u8 r, g, b, a;
} GXColor;

void GXSetTevColor(u32 id, GXColor color);

extern u32 gc_gx_tev_color_reg_ra_last;
extern u32 gc_gx_tev_color_reg_bg_last;
extern u32 gc_gx_last_ras_reg;

const char *gc_scenario_label(void) { return "GXSetTevColor/mp4_wipe_c0"; }
const char *gc_scenario_out_path(void) { return "../actual/gx_set_tev_color_mp4_wipe_c0_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    GXColor c = { 0x12, 0x34, 0x56, 0x78 };
    GXSetTevColor(1, c);

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x40);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0x00, 0xDEADBEEFu);
    wr32be(p + 0x04, gc_gx_tev_color_reg_ra_last);
    wr32be(p + 0x08, gc_gx_tev_color_reg_bg_last);
    wr32be(p + 0x0C, gc_gx_last_ras_reg);
    for (u32 off = 0x10; off < 0x40; off += 4) {
        wr32be(p + off, 0);
    }
}

