#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

extern uint32_t gc_gx_pos1x16_last;

typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;

typedef struct { uint8_t _dummy; } GXFifoObj;
GXFifoObj *GXInit(void *base, u32 size);
void GXPosition1x16(u16 x);

const char *gc_scenario_label(void) { return "GXPosition1x16/mp4_draw"; }
const char *gc_scenario_out_path(void) { return "../actual/gx_position_1x16_mp4_draw_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    static u8 fifo[0x20000];
    GXInit(fifo, sizeof(fifo));

    GXPosition1x16((u16)0x1234);

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x08);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0x00, 0xDEADBEEFu);
    wr32be(p + 0x04, gc_gx_pos1x16_last);
}
