#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

extern uint32_t gc_gx_pos2f32_x_bits;
extern uint32_t gc_gx_pos2f32_y_bits;

typedef uint32_t u32;
typedef uint8_t u8;

typedef struct { uint8_t _dummy; } GXFifoObj;
GXFifoObj *GXInit(void *base, u32 size);
void GXPosition2f32(float x, float y);

const char *gc_scenario_label(void) { return "GXPosition2f32/mp4_pfdrawfonts"; }
const char *gc_scenario_out_path(void) { return "../actual/gx_position_2f32_mp4_pfdrawfonts_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    static u8 fifo[0x20000];
    GXInit(fifo, sizeof(fifo));

    GXPosition2f32(64.0f, 48.0f);

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x0C);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0x00, 0xDEADBEEFu);
    wr32be(p + 0x04, gc_gx_pos2f32_x_bits);
    wr32be(p + 0x08, gc_gx_pos2f32_y_bits);
}

