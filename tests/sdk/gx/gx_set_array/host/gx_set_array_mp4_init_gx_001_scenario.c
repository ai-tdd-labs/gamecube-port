#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

extern uint32_t gc_gx_array_base[32];
extern uint32_t gc_gx_array_stride[32];

typedef uint32_t u32;
typedef uint8_t u8;

typedef struct { uint8_t _dummy; } GXFifoObj;
GXFifoObj *GXInit(void *base, u32 size);
void GXSetArray(u32 attr, const void *base_ptr, u8 stride);

enum { GX_VA_POS = 9 };

const char *gc_scenario_label(void) { return "GXSetArray/mp4_init_gx"; }
const char *gc_scenario_out_path(void) { return "../actual/gx_set_array_mp4_init_gx_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    static u8 fifo[0x20000];
    GXInit(fifo, sizeof(fifo));

    GXSetArray(GX_VA_POS, (const void*)0x81234567u, 0x20);

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x0C);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0x00, 0xDEADBEEFu);
    wr32be(p + 0x04, gc_gx_array_base[0]);
    wr32be(p + 0x08, gc_gx_array_stride[0]);
}
