#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

extern uint32_t gc_gx_vcache_sel;
void GXReadVCacheMetric(uint32_t *check, uint32_t *miss, uint32_t *stall);

const char *gc_scenario_label(void) { return "GXReadVCacheMetric/mp4_main_loop"; }
const char *gc_scenario_out_path(void) { return "../actual/gx_read_vcache_metric_mp4_main_loop_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    gc_gx_vcache_sel = 0x12345678u;

    uint32_t a = 0, b = 0, c = 0;
    GXReadVCacheMetric(&a, &b, &c);

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x14);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0x00, 0xDEADBEEFu);
    wr32be(p + 0x04, a);
    wr32be(p + 0x08, b);
    wr32be(p + 0x0C, c);
}
