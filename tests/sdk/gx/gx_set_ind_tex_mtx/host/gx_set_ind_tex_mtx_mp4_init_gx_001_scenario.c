#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

typedef int8_t s8;
typedef uint32_t u32;
typedef float f32;

void GXSetIndTexMtx(u32 mtx_id, f32 offset[2][3], s8 scale_exp);
extern u32 gc_gx_ind_mtx_reg0;
extern u32 gc_gx_ind_mtx_reg1;
extern u32 gc_gx_ind_mtx_reg2;
extern u32 gc_gx_last_ras_reg;
extern u32 gc_gx_bp_sent_not;

const char *gc_scenario_label(void) { return "gx_set_ind_tex_mtx/mp4_init_gx"; }
const char *gc_scenario_out_path(void) {
    return "../actual/gx_set_ind_tex_mtx_mp4_init_gx_001.bin";
}

void gc_scenario_run(GcRam *ram) {
    f32 off[2][3] = {
        { 0.125f, -0.25f, 0.5f },
        { 0.75f, -0.5f, 0.25f },
    };
    gc_gx_ind_mtx_reg0 = 0;
    gc_gx_ind_mtx_reg1 = 0;
    gc_gx_ind_mtx_reg2 = 0;
    gc_gx_last_ras_reg = 0;
    gc_gx_bp_sent_not = 1;

    GXSetIndTexMtx(1u, off, 2);

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x18);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0x00, 0xDEADBEEFu);
    wr32be(p + 0x04, gc_gx_ind_mtx_reg0);
    wr32be(p + 0x08, gc_gx_ind_mtx_reg1);
    wr32be(p + 0x0C, gc_gx_ind_mtx_reg2);
    wr32be(p + 0x10, gc_gx_last_ras_reg);
    wr32be(p + 0x14, gc_gx_bp_sent_not);
}
