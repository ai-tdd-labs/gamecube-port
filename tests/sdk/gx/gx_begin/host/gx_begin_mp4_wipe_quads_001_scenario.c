#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;

void GXBegin(u8 type, u8 vtxfmt, u16 nverts);

extern u32 gc_gx_fifo_begin_u8;
extern u32 gc_gx_fifo_begin_u16;

const char *gc_scenario_label(void) { return "GXBegin/mp4_wipe_quads"; }
const char *gc_scenario_out_path(void) { return "../actual/gx_begin_mp4_wipe_quads_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    enum { GX_QUADS = 0x80, GX_VTXFMT0 = 0 };
    GXBegin((u8)GX_QUADS, (u8)GX_VTXFMT0, (u16)4);

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x40);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0x00, 0xDEADBEEFu);
    wr32be(p + 0x04, gc_gx_fifo_begin_u8);
    wr32be(p + 0x08, gc_gx_fifo_begin_u16);
    for (u32 off = 0x0C; off < 0x40; off += 4) {
        wr32be(p + off, 0);
    }
}

