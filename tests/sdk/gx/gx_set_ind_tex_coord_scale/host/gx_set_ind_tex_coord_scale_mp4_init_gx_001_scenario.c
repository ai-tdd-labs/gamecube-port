#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

typedef uint32_t u32;

void GXSetIndTexCoordScale(u32 ind_state, u32 scale_s, u32 scale_t);

extern u32 gc_gx_ind_tex_scale0;
extern u32 gc_gx_ind_tex_scale1;
extern u32 gc_gx_last_ras_reg;
extern u32 gc_gx_bp_sent_not;

const char *gc_scenario_label(void) { return "gx_set_ind_tex_coord_scale/mp4_init_gx"; }
const char *gc_scenario_out_path(void) {
    return "../actual/gx_set_ind_tex_coord_scale_mp4_init_gx_001.bin";
}

void gc_scenario_run(GcRam *ram) {
    gc_gx_ind_tex_scale0 = 0x12340000u;
    gc_gx_ind_tex_scale1 = 0x56780000u;
    gc_gx_last_ras_reg = 0;
    gc_gx_bp_sent_not = 1;

    GXSetIndTexCoordScale(2u, 1u, 1u);

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x14);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0x00, 0xDEADBEEFu);
    wr32be(p + 0x04, gc_gx_ind_tex_scale0);
    wr32be(p + 0x08, gc_gx_ind_tex_scale1);
    wr32be(p + 0x0C, gc_gx_last_ras_reg);
    wr32be(p + 0x10, gc_gx_bp_sent_not);
}
