#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

extern uint32_t gc_gx_su_scis0;
extern uint32_t gc_gx_su_scis1;
extern uint32_t gc_gx_bp_sent_not;

typedef uint32_t u32;

typedef struct { uint8_t _dummy; } GXFifoObj;
GXFifoObj *GXInit(void *base, u32 size);
void GXSetScissor(u32 left, u32 top, u32 wd, u32 ht);

const char *gc_scenario_label(void) { return "GXSetScissor/mp4_init_gx"; }
const char *gc_scenario_out_path(void) { return "../actual/gx_set_scissor_mp4_init_gx_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    static uint8_t fifo[0x20000];
    GXInit(fifo, sizeof(fifo));

    GXSetScissor(0, 0, 640, 448);

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x10);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0x00, 0xDEADBEEFu);
    wr32be(p + 0x04, gc_gx_su_scis0);
    wr32be(p + 0x08, gc_gx_su_scis1);
    wr32be(p + 0x0C, gc_gx_bp_sent_not);
}
