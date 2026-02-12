#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

typedef uint8_t u8;
typedef uint32_t u32;

void GXSetTevIndWarp(u32 tev_stage, u32 ind_stage, u8 signed_offset, u8 replace_mode, u32 matrix_sel);
extern u32 gc_gx_last_ras_reg;
extern u32 gc_gx_bp_sent_not;

const char *gc_scenario_label(void) { return "gx_set_tev_ind_warp/mp4_init_gx"; }
const char *gc_scenario_out_path(void) {
    return "../actual/gx_set_tev_ind_warp_mp4_init_gx_001.bin";
}

void gc_scenario_run(GcRam *ram) {
    gc_gx_last_ras_reg = 0;
    gc_gx_bp_sent_not = 1;
    GXSetTevIndWarp(2u, 1u, 1u, 1u, 2u);

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x0C);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0x00, 0xDEADBEEFu);
    wr32be(p + 0x04, gc_gx_last_ras_reg);
    wr32be(p + 0x08, gc_gx_bp_sent_not);
}
