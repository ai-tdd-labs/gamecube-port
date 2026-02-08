#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

typedef uint32_t u32;

void GXSetDrawDone(void);

extern u32 gc_gx_set_draw_done_calls;
extern u32 gc_gx_last_ras_reg;
extern u32 gc_gx_draw_done_flag;

const char *gc_scenario_label(void) { return "GXSetDrawDone/mp4_hu3d_exec"; }
const char *gc_scenario_out_path(void) { return "../actual/gx_set_draw_done_mp4_hu3d_exec_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    gc_gx_set_draw_done_calls = 0;
    gc_gx_last_ras_reg = 0;
    gc_gx_draw_done_flag = 0xFFFFFFFFu;

    GXSetDrawDone();

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x10);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0x00, 0xDEADBEEFu);
    wr32be(p + 0x04, gc_gx_set_draw_done_calls);
    wr32be(p + 0x08, gc_gx_last_ras_reg);
    wr32be(p + 0x0C, gc_gx_draw_done_flag);
}
