#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

typedef uint32_t u32;
typedef float f32;

void GXLoadPosMtxImm(f32 mtx[3][4], u32 id);

extern u32 gc_gx_fifo_u8_last;
extern u32 gc_gx_fifo_u32_last;
extern u32 gc_gx_fifo_mtx_words[12];

const char *gc_scenario_label(void) { return "GXLoadPosMtxImm/mp4_wipe_identity"; }
const char *gc_scenario_out_path(void) { return "../actual/gx_load_pos_mtx_imm_mp4_wipe_identity_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    f32 mtx[3][4];
    u32 r, c;
    for (r = 0; r < 3; r++) {
        for (c = 0; c < 4; c++) {
            mtx[r][c] = 0.0f;
        }
    }
    mtx[0][0] = 1.0f;
    mtx[1][1] = 1.0f;
    mtx[2][2] = 1.0f;

    GXLoadPosMtxImm(mtx, 0);

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
    for (u32 off = 0x20; off < 0x40; off += 4) {
        wr32be(p + off, 0);
    }
}

