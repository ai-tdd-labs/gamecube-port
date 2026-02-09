#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

extern uint32_t gc_gx_pos2u16_x;
extern uint32_t gc_gx_pos2u16_y;

typedef uint32_t u32;
typedef uint8_t u8;
typedef uint16_t u16;

typedef struct { uint8_t _dummy; } GXFifoObj;
GXFifoObj *GXInit(void *base, u32 size);
void GXPosition2u16(u16 x, u16 y);

const char *gc_scenario_label(void) { return "GXPosition2u16/mp4_wipe"; }
const char *gc_scenario_out_path(void) { return "../actual/gx_position_2u16_mp4_wipe_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    static u8 fifo[0x20000];
    GXInit(fifo, sizeof(fifo));

    GXPosition2u16((u16)0x1234u, (u16)0xFFFFu);

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x0C);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0x00, 0xDEADBE02u);
    wr32be(p + 0x04, gc_gx_pos2u16_x);
    wr32be(p + 0x08, gc_gx_pos2u16_y);
}
