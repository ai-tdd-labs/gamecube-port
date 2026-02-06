#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

extern uint32_t gc_gx_pix_metrics[6];
void GXReadPixMetric(uint32_t *a, uint32_t *b, uint32_t *c, uint32_t *d, uint32_t *e, uint32_t *f);

const char *gc_scenario_label(void) { return "GXReadPixMetric/mp4_main_loop"; }
const char *gc_scenario_out_path(void) { return "../actual/gx_read_pix_metric_mp4_main_loop_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    gc_gx_pix_metrics[0] = 11; gc_gx_pix_metrics[1] = 22; gc_gx_pix_metrics[2] = 33;
    gc_gx_pix_metrics[3] = 44; gc_gx_pix_metrics[4] = 55; gc_gx_pix_metrics[5] = 66;

    uint32_t a=0,b=0,c=0,d=0,e=0,f=0;
    GXReadPixMetric(&a,&b,&c,&d,&e,&f);

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x1C);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0x00, 0xDEADBEEFu);
    wr32be(p + 0x04, a);
    wr32be(p + 0x08, b);
    wr32be(p + 0x0C, c);
    wr32be(p + 0x10, d);
    wr32be(p + 0x14, e);
    wr32be(p + 0x18, f);
}
