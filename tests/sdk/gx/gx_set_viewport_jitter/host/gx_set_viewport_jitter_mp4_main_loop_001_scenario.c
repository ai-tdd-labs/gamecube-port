#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

extern float gc_gx_vp_top;
void GXSetViewportJitter(float left, float top, float wd, float ht, float nearz, float farz, uint32_t field);

const char *gc_scenario_label(void) { return "GXSetViewportJitter/mp4_main_loop"; }
const char *gc_scenario_out_path(void) { return "../actual/gx_set_viewport_jitter_mp4_main_loop_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    GXSetViewportJitter(0, 0, 640, 480, 0, 1, 0);

    union { float f; uint32_t u; } u = { .f = gc_gx_vp_top };
    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 8);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0, 0xDEADBEEFu);
    wr32be(p + 4, u.u);
}
