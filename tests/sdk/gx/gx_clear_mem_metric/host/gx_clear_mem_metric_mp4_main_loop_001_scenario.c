#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

extern uint32_t gc_gx_mem_metrics[10];
void GXClearMemMetric(void);

const char *gc_scenario_label(void) { return "GXClearMemMetric/mp4_main_loop"; }
const char *gc_scenario_out_path(void) { return "../actual/gx_clear_mem_metric_mp4_main_loop_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    for (uint32_t i = 0; i < 10; i++) gc_gx_mem_metrics[i] = i + 1;
    GXClearMemMetric();

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x2C);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0x00, 0xDEADBEEFu);
    for (uint32_t i = 0; i < 10; i++) wr32be(p + 0x04 + i*4, gc_gx_mem_metrics[i]);
}
