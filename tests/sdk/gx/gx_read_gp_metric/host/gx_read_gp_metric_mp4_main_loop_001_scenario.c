#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

extern uint32_t gc_gx_gp_perf0;
extern uint32_t gc_gx_gp_perf1;
void GXReadGPMetric(uint32_t *met0, uint32_t *met1);

const char *gc_scenario_label(void) { return "GXReadGPMetric/mp4_main_loop"; }
const char *gc_scenario_out_path(void) { return "../actual/gx_read_gp_metric_mp4_main_loop_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    gc_gx_gp_perf0 = 0x11111111u;
    gc_gx_gp_perf1 = 0x22222222u;

    uint32_t a = 0, b = 0;
    GXReadGPMetric(&a, &b);

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x10);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0x00, 0xDEADBEEFu);
    wr32be(p + 0x04, a);
    wr32be(p + 0x08, b);
}
