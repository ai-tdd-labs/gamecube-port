#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

typedef uint32_t u32;
typedef float f32;

void GXLoadNrmMtxImm(f32 mtx[3][4], u32 id);

extern u32 gc_gx_fifo_u8_last;
extern u32 gc_gx_fifo_u32_last;
extern u32 gc_gx_fifo_mtx_words[12];

const char *gc_scenario_label(void) { return "GXLoadNrmMtxImm/mp4_hsfdraw"; }
const char *gc_scenario_out_path(void) { return "../actual/gx_load_nrm_mtx_imm_mp4_hsfdraw_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    f32 mtx[3][4];
    u32 r, c;
    for (r = 0; r < 3; r++) {
        for (c = 0; c < 4; c++) {
            mtx[r][c] = 0.0f;
        }
    }

    mtx[0][0] = 1.0f;
    mtx[0][1] = 2.0f;
    mtx[0][2] = 3.0f;
    mtx[1][0] = 4.0f;
    mtx[1][1] = 5.0f;
    mtx[1][2] = 6.0f;
    mtx[2][0] = 7.0f;
    mtx[2][1] = 8.0f;
    mtx[2][2] = 9.0f;
    mtx[0][3] = 123.0f;
    mtx[1][3] = 456.0f;
    mtx[2][3] = 789.0f;

    GXLoadNrmMtxImm(mtx, 0);

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x40);
    if (!p) die("gc_ram_ptr failed");

    wr32be(p + 0x00, 0xDEADBEEFu);
    wr32be(p + 0x04, gc_gx_fifo_u8_last);
    wr32be(p + 0x08, gc_gx_fifo_u32_last);

    wr32be(p + 0x0C, gc_gx_fifo_mtx_words[0]);
    wr32be(p + 0x10, gc_gx_fifo_mtx_words[1]);
    wr32be(p + 0x14, gc_gx_fifo_mtx_words[2]);
    wr32be(p + 0x18, gc_gx_fifo_mtx_words[3]);
    wr32be(p + 0x1C, gc_gx_fifo_mtx_words[4]);
    wr32be(p + 0x20, gc_gx_fifo_mtx_words[5]);
    wr32be(p + 0x24, gc_gx_fifo_mtx_words[6]);
    wr32be(p + 0x28, gc_gx_fifo_mtx_words[7]);
    wr32be(p + 0x2C, gc_gx_fifo_mtx_words[8]);

    for (u32 off = 0x30; off < 0x40; off += 4) {
        wr32be(p + off, 0);
    }
}
