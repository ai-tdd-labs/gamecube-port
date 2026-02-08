#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

typedef uint32_t u32;

void GXSetCurrentMtx(u32 id);

extern u32 gc_gx_mat_idx_a;
extern u32 gc_gx_bp_sent_not;
extern u32 gc_gx_xf_regs[32];

const char *gc_scenario_label(void) { return "GXSetCurrentMtx/mp4_hu3d_exec"; }
const char *gc_scenario_out_path(void) { return "../actual/gx_set_current_mtx_mp4_hu3d_exec_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    gc_gx_mat_idx_a = 0;
    gc_gx_bp_sent_not = 0;
    for (u32 i = 0; i < 32; i++) gc_gx_xf_regs[i] = 0;

    GXSetCurrentMtx(0u);

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x10);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0x00, 0xDEADBEEFu);
    wr32be(p + 0x04, gc_gx_mat_idx_a);
    wr32be(p + 0x08, gc_gx_xf_regs[24]);
    wr32be(p + 0x0C, gc_gx_bp_sent_not);
}
