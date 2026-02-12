#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

typedef uint32_t u32;

void GXSetIndTexOrder(u32 ind_stage, u32 tex_coord, u32 tex_map);

extern u32 gc_gx_iref;
extern u32 gc_gx_last_ras_reg;
extern u32 gc_gx_dirty_state;
extern u32 gc_gx_bp_sent_not;

const char *gc_scenario_label(void) { return "gx_set_ind_tex_order/mp4_init_gx"; }
const char *gc_scenario_out_path(void) {
    return "../actual/gx_set_ind_tex_order_mp4_init_gx_001.bin";
}

void gc_scenario_run(GcRam *ram) {
    gc_gx_iref = 0x44000000u;
    gc_gx_last_ras_reg = 0;
    gc_gx_dirty_state = 0x00000008u;
    gc_gx_bp_sent_not = 1;

    GXSetIndTexOrder(1u, 2u, 3u);

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x14);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0x00, 0xDEADBEEFu);
    wr32be(p + 0x04, gc_gx_iref);
    wr32be(p + 0x08, gc_gx_last_ras_reg);
    wr32be(p + 0x0C, gc_gx_dirty_state);
    wr32be(p + 0x10, gc_gx_bp_sent_not);
}
