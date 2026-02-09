#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

typedef uint32_t u32;
typedef float f32;

enum { GX_MTX3x4 = 0, GX_MTX2x4 = 1, GX_TEXMTX7 = 51 };

void GXLoadTexMtxImm(f32 mtx[][4], u32 id, u32 type);

extern u32 gc_gx_fifo_u8_last;
extern u32 gc_gx_fifo_u32_last;
extern u32 gc_gx_fifo_mtx_words[12];

const char *gc_scenario_label(void) { return "GXLoadTexMtxImm/mp4_hsfdraw_2x4"; }
const char *gc_scenario_out_path(void) { return "../actual/gx_load_tex_mtx_imm_mp4_hsfdraw_2x4_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    static f32 mtx[2][4];
    u32 r, c;
    for (r = 0; r < 2; r++) {
        for (c = 0; c < 4; c++) {
            mtx[r][c] = (f32)(r * 10 + c + 1);
        }
    }

    GXLoadTexMtxImm((f32 (*)[4])mtx, GX_TEXMTX7, GX_MTX2x4);

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x40);
    if (!p) die("gc_ram_ptr failed");

    wr32be(p + 0x00, 0xDEADBEEFu);
    wr32be(p + 0x04, gc_gx_fifo_u8_last);
    wr32be(p + 0x08, gc_gx_fifo_u32_last);

    for (u32 i = 0; i < 8; i++) {
        wr32be(p + 0x0C + i * 4u, gc_gx_fifo_mtx_words[i]);
    }
    for (u32 off = 0x2C; off < 0x40; off += 4) {
        wr32be(p + off, 0);
    }
}
