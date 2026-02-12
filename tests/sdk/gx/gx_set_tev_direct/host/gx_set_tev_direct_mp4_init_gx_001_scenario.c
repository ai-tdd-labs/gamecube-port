#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

typedef uint32_t u32;

void GXSetTevDirect(u32 tev_stage);
extern u32 gc_gx_last_ras_reg;
extern u32 gc_gx_bp_sent_not;

const char *gc_scenario_label(void) { return "gx_set_tev_direct/mp4_init_gx"; }
const char *gc_scenario_out_path(void) {
    return "../actual/gx_set_tev_direct_mp4_init_gx_001.bin";
}

void gc_scenario_run(GcRam *ram) {
    gc_gx_last_ras_reg = 0;
    gc_gx_bp_sent_not = 1;
    GXSetTevDirect(5u);

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x0C);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0x00, 0xDEADBEEFu);
    wr32be(p + 0x04, gc_gx_last_ras_reg);
    wr32be(p + 0x08, gc_gx_bp_sent_not);
}
