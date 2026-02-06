#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

extern uint32_t gc_gx_cp_disp_stride;

typedef uint32_t u32;
typedef uint16_t u16;

typedef struct { uint8_t _dummy; } GXFifoObj;
GXFifoObj *GXInit(void *base, u32 size);
void GXSetDispCopyDst(u16 wd, u16 ht);

const char *gc_scenario_label(void) { return "GXSetDispCopyDst/mp4_init_gx"; }
const char *gc_scenario_out_path(void) { return "../actual/gx_set_disp_copy_dst_mp4_init_gx_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    static uint8_t fifo[0x20000];
    GXInit(fifo, sizeof(fifo));

    GXSetDispCopyDst(640, 464);

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 8);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0x00, 0xDEADBEEFu);
    wr32be(p + 0x04, gc_gx_cp_disp_stride);
}
