#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

extern uint32_t gc_gx_pix_metrics[6];
void GXClearPixMetric(void);

const char *gc_scenario_label(void) { return "GXClearPixMetric/mp4_main_loop"; }
const char *gc_scenario_out_path(void) { return "../actual/gx_clear_pix_metric_mp4_main_loop_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    for (uint32_t i = 0; i < 6; i++) gc_gx_pix_metrics[i] = i + 1;
    GXClearPixMetric();

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x1C);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0x00, 0xDEADBEEFu);
    for (uint32_t i = 0; i < 6; i++) wr32be(p + 0x04 + i*4, gc_gx_pix_metrics[i]);
}
