#include <stdint.h>
#include <string.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

typedef uint32_t u32;
typedef float f32;

typedef enum {
    GX_PERSPECTIVE = 0,
    GX_ORTHOGRAPHIC = 1,
} GXProjectionType;

void GXSetProjection(f32 mtx[4][4], GXProjectionType type);

extern u32 gc_gx_proj_type;
extern u32 gc_gx_proj_mtx_bits[6];
extern u32 gc_gx_xf_regs[64];
extern u32 gc_gx_bp_sent_not;

static void fill_ortho_0_1(f32 mtx[4][4]) {
    memset(mtx, 0, sizeof(f32) * 16);
    // For C_MTXOrtho(0,1,0,1,0,1) only these are read by GXSetProjection:
    // mtx[0][0]=2, mtx[0][3]=-1, mtx[1][1]=2, mtx[1][3]=-1, mtx[2][2]=-2, mtx[2][3]=-1
    mtx[0][0] = 2.0f;
    mtx[0][3] = -1.0f;
    mtx[1][1] = 2.0f;
    mtx[1][3] = -1.0f;
    mtx[2][2] = -2.0f;
    mtx[2][3] = -1.0f;
}

const char *gc_scenario_label(void) { return "GXSetProjection/mp4_shadow_ortho"; }
const char *gc_scenario_out_path(void) { return "../actual/gx_set_projection_mp4_shadow_ortho_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    f32 mtx[4][4];
    fill_ortho_0_1(mtx);

    gc_gx_proj_type = 0;
    for (u32 i = 0; i < 6; i++) gc_gx_proj_mtx_bits[i] = 0;
    for (u32 i = 0; i < 64; i++) gc_gx_xf_regs[i] = 0;
    gc_gx_bp_sent_not = 0;

    GXSetProjection(mtx, GX_ORTHOGRAPHIC);

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x40);
    if (!p) die("gc_ram_ptr failed");

    wr32be(p + 0x00, 0xDEADBEEFu);
    wr32be(p + 0x04, gc_gx_proj_type);
    for (u32 i = 0; i < 6; i++) wr32be(p + 0x08 + i * 4, gc_gx_proj_mtx_bits[i]);
    wr32be(p + 0x20, gc_gx_xf_regs[32]);
    wr32be(p + 0x24, gc_gx_xf_regs[33]);
    wr32be(p + 0x28, gc_gx_xf_regs[34]);
    wr32be(p + 0x2C, gc_gx_xf_regs[35]);
    wr32be(p + 0x30, gc_gx_xf_regs[36]);
    wr32be(p + 0x34, gc_gx_xf_regs[37]);
    wr32be(p + 0x38, gc_gx_xf_regs[38]);
    wr32be(p + 0x3C, gc_gx_bp_sent_not);
}
