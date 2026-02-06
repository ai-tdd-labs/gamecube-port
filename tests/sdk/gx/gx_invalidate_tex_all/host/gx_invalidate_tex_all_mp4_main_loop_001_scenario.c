#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

extern uint32_t gc_gx_invalidate_tex_all_calls;
void GXInvalidateTexAll(void);

const char *gc_scenario_label(void) { return "GXInvalidateTexAll/mp4_main_loop"; }
const char *gc_scenario_out_path(void) { return "../actual/gx_invalidate_tex_all_mp4_main_loop_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    gc_gx_invalidate_tex_all_calls = 0;
    GXInvalidateTexAll();

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 8);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0, 0xDEADBEEFu);
    wr32be(p + 4, gc_gx_invalidate_tex_all_calls);
}
