#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

// Mirror a small subset of GXData used by the testcase.
extern uint32_t gc_gx_bp_sent_not;
extern float gc_gx_vp_left;
extern float gc_gx_vp_top;
extern float gc_gx_vp_wd;
extern float gc_gx_vp_ht;

typedef uint32_t u32;

typedef struct { uint8_t _dummy; } GXFifoObj;
GXFifoObj *GXInit(void *base, u32 size);
void GXSetViewport(float left, float top, float wd, float ht, float nearz, float farz);

const char *gc_scenario_label(void) { return "GXSetViewport/mp4_init_gx"; }
const char *gc_scenario_out_path(void) { return "../actual/gx_set_viewport_mp4_init_gx_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    static uint8_t fifo[0x20000];
    GXInit(fifo, sizeof(fifo));

    GXSetViewport(0.0f, 0.0f, 640.0f, 464.0f, 0.0f, 1.0f);

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x18);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0x00, 0xDEADBEEFu);
    wr32be(p + 0x04, gc_gx_bp_sent_not);
    wr32be(p + 0x08, (u32)gc_gx_vp_left);
    wr32be(p + 0x0C, (u32)gc_gx_vp_top);
    wr32be(p + 0x10, (u32)gc_gx_vp_wd);
    wr32be(p + 0x14, (u32)gc_gx_vp_ht);
}
