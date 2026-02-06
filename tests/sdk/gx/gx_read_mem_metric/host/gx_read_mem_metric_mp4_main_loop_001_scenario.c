#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

extern uint32_t gc_gx_mem_metrics[10];
void GXReadMemMetric(uint32_t *a,uint32_t *b,uint32_t *c,uint32_t *d,uint32_t *e,uint32_t *f,uint32_t *g,uint32_t *h,uint32_t *i,uint32_t *j);

const char *gc_scenario_label(void) { return "GXReadMemMetric/mp4_main_loop"; }
const char *gc_scenario_out_path(void) { return "../actual/gx_read_mem_metric_mp4_main_loop_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    gc_gx_mem_metrics[0]=101; gc_gx_mem_metrics[1]=102; gc_gx_mem_metrics[2]=103; gc_gx_mem_metrics[3]=104; gc_gx_mem_metrics[4]=105;
    gc_gx_mem_metrics[5]=106; gc_gx_mem_metrics[6]=107; gc_gx_mem_metrics[7]=108; gc_gx_mem_metrics[8]=109; gc_gx_mem_metrics[9]=110;

    uint32_t a=0,b=0,c=0,d=0,e=0,f=0,g=0,h=0,i=0,j=0;
    GXReadMemMetric(&a,&b,&c,&d,&e,&f,&g,&h,&i,&j);

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x2C);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0x00, 0xDEADBEEFu);
    wr32be(p + 0x04, a);
    wr32be(p + 0x08, b);
    wr32be(p + 0x0C, c);
    wr32be(p + 0x10, d);
    wr32be(p + 0x14, e);
    wr32be(p + 0x18, f);
    wr32be(p + 0x1C, g);
    wr32be(p + 0x20, h);
    wr32be(p + 0x24, i);
    wr32be(p + 0x28, j);
}
