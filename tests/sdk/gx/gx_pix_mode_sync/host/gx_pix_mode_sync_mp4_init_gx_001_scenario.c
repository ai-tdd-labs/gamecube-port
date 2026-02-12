#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

typedef uint32_t u32;
typedef uint8_t u8;

void GXSetZCompLoc(u8 before_tex);
void GXPixModeSync(void);

extern u32 gc_gx_bp_sent_not;
extern u32 gc_gx_last_ras_reg;
extern u32 gc_gx_pe_ctrl;

const char *gc_scenario_label(void) { return "gx_pix_mode_sync/mp4_init_gx"; }
const char *gc_scenario_out_path(void) {
    return "../actual/gx_pix_mode_sync_mp4_init_gx_001.bin";
}

void gc_scenario_run(GcRam *ram) {
    gc_gx_bp_sent_not = 0;
    gc_gx_last_ras_reg = 0;
    gc_gx_pe_ctrl = 0;

    GXSetZCompLoc(1u);
    gc_gx_bp_sent_not = 1;
    GXPixModeSync();

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x10);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0x00, 0xDEADBEEFu);
    wr32be(p + 0x04, gc_gx_pe_ctrl);
    wr32be(p + 0x08, gc_gx_last_ras_reg);
    wr32be(p + 0x0C, gc_gx_bp_sent_not);
}
