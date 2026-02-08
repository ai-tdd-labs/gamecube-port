#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

typedef uint32_t u32;
typedef uint8_t u8;

void GXSetTexCoordGen(u8 dst_coord, u8 func, u8 src_param, u32 mtx);

extern u32 gc_gx_xf_texcoordgen_40[8];
extern u32 gc_gx_xf_texcoordgen_50[8];
extern u32 gc_gx_mat_idx_a;
extern u32 gc_gx_xf_regs[64];

const char *gc_scenario_label(void) { return "GXSetTexCoordGen/mp4_wipe_frame_still"; }
const char *gc_scenario_out_path(void) { return "../actual/gx_set_tex_coord_gen_mp4_wipe_frame_still_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    enum {
        GX_TEXCOORD0 = 0,
        GX_TG_MTX2x4 = 1,
        GX_TG_TEX0 = 4,
        GX_IDENTITY = 60,
    };

    GXSetTexCoordGen((u8)GX_TEXCOORD0, (u8)GX_TG_MTX2x4, (u8)GX_TG_TEX0, (u32)GX_IDENTITY);

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x40);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0x00, 0xDEADBEEFu);
    wr32be(p + 0x04, gc_gx_xf_texcoordgen_40[0]);
    wr32be(p + 0x08, gc_gx_xf_texcoordgen_50[0]);
    wr32be(p + 0x0C, gc_gx_mat_idx_a);
    wr32be(p + 0x10, gc_gx_xf_regs[24]);
    for (u32 off = 0x14; off < 0x40; off += 4) {
        wr32be(p + off, 0);
    }
}

