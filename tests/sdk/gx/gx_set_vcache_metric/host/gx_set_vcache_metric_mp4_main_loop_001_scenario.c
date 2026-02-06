#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

extern uint32_t gc_gx_vcache_sel;
void GXSetVCacheMetric(uint32_t attr);

const char *gc_scenario_label(void) { return "GXSetVCacheMetric/mp4_main_loop"; }
const char *gc_scenario_out_path(void) { return "../actual/gx_set_vcache_metric_mp4_main_loop_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    GXSetVCacheMetric(0x99u);

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x08);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0x00, 0xDEADBEEFu);
    wr32be(p + 0x04, gc_gx_vcache_sel);
}
