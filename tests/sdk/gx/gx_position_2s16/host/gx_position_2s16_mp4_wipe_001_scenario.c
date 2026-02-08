#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

extern uint32_t gc_gx_pos2s16_x;
extern uint32_t gc_gx_pos2s16_y;

typedef uint32_t u32;
typedef uint8_t u8;
typedef int16_t s16;

typedef struct { uint8_t _dummy; } GXFifoObj;
GXFifoObj *GXInit(void *base, u32 size);
void GXPosition2s16(s16 x, s16 y);

const char *gc_scenario_label(void) { return "GXPosition2s16/mp4_wipe"; }
const char *gc_scenario_out_path(void) { return "../actual/gx_position_2s16_mp4_wipe_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    static u8 fifo[0x20000];
    GXInit(fifo, sizeof(fifo));

    GXPosition2s16((s16)-12, (s16)345);

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x0C);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0x00, 0xDEADBEEFu);
    wr32be(p + 0x04, gc_gx_pos2s16_x);
    wr32be(p + 0x08, gc_gx_pos2s16_y);
}

