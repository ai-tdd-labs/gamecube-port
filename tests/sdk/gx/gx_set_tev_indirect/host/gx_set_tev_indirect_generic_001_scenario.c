#include <stdint.h>
#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t  u8;
typedef int32_t  s32;
typedef int8_t   s8;

typedef s32 GXTevStageID;
typedef s32 GXIndTexStageID;
typedef s32 GXIndTexFormat;
typedef s32 GXIndTexBiasSel;
typedef s32 GXIndTexMtxID;
typedef s32 GXIndTexWrap;
typedef s32 GXIndTexAlphaSel;
typedef s32 GXIndTexScale;

void GXSetTevIndirect(GXTevStageID tev_stage, GXIndTexStageID ind_stage,
                      GXIndTexFormat format, GXIndTexBiasSel bias_sel,
                      GXIndTexMtxID matrix_sel,
                      GXIndTexWrap wrap_s, GXIndTexWrap wrap_t,
                      u8 add_prev, u8 utc_lod,
                      GXIndTexAlphaSel alpha_sel);
void GXSetTevDirect(GXTevStageID tev_stage);
void GXSetTevIndWarp(GXTevStageID tev_stage, GXIndTexStageID ind_stage,
                     u8 signed_offset, u8 replace_mode,
                     GXIndTexMtxID matrix_sel);
void GXSetIndTexMtx(GXIndTexMtxID mtx_id, float offset[2][3], s8 scale_exp);
void GXSetIndTexOrder(GXIndTexStageID ind_stage, u32 tex_coord, u32 tex_map);
void GXSetIndTexCoordScale(GXIndTexStageID ind_stage,
                           GXIndTexScale scale_s, GXIndTexScale scale_t);
void GXSetNumIndStages(u8 nIndStages);
void GXSetTevIndTile(GXTevStageID tev_stage, GXIndTexStageID ind_stage,
                     u16 tilesize_s, u16 tilesize_t,
                     u16 tilespacing_s, u16 tilespacing_t,
                     GXIndTexFormat format, GXIndTexMtxID matrix_sel,
                     GXIndTexBiasSel bias_sel, GXIndTexAlphaSel alpha_sel);

extern u32 gc_gx_gen_mode;
extern u32 gc_gx_iref;
extern u32 gc_gx_ind_tex_scale0;
extern u32 gc_gx_ind_tex_scale1;
extern u32 gc_gx_tev_ind[16];
extern u32 gc_gx_ind_mtx[9];
extern u32 gc_gx_dirty_state;
extern u32 gc_gx_bp_sent_not;

const char *gc_scenario_label(void) { return "GXSetTevIndirect/generic"; }
const char *gc_scenario_out_path(void) { return "../actual/gx_set_tev_indirect_generic_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    (void)ram;

    // 1. GXSetNumIndStages(2)
    GXSetNumIndStages(2);

    // 2. GXSetIndTexOrder
    GXSetIndTexOrder(0, 5, 3);
    GXSetIndTexOrder(1, 2, 1);

    // 3. GXSetIndTexCoordScale
    GXSetIndTexCoordScale(0, 3, 5);
    GXSetIndTexCoordScale(1, 2, 4);

    // 4. GXSetIndTexMtx — matrix 0 (GX_ITM_0=1)
    float mtx0[2][3] = { {0.5f, 0.25f, 0.0f}, {0.0f, 0.75f, 0.125f} };
    GXSetIndTexMtx(1, mtx0, 3);

    // 5. GXSetTevIndirect — stage 0
    GXSetTevIndirect(0, 0, 0, 7, 1, 3, 2, 1, 0, 1);

    // 6. GXSetTevDirect — stage 1
    GXSetTevDirect(1);

    // 7. GXSetTevIndWarp — stage 2
    GXSetTevIndWarp(2, 1, 1, 0, 1);

    // 8. GXSetTevIndTile — stage 3
    GXSetTevIndTile(3, 0, 64, 32, 128, 64, 0, 2, 7, 0);

    // Dump state.
    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x100);
    if (!p) die("gc_ram_ptr failed");
    for (u32 i = 0; i < 0x100; i += 4) wr32be(p + i, 0);

    u32 off = 0;
    wr32be(p + off, 0xDEADBEEFu); off += 4;
    wr32be(p + off, gc_gx_gen_mode); off += 4;
    wr32be(p + off, gc_gx_iref); off += 4;
    wr32be(p + off, gc_gx_ind_tex_scale0); off += 4;
    wr32be(p + off, gc_gx_ind_tex_scale1); off += 4;
    for (u32 i = 0; i < 4; i++) { wr32be(p + off, gc_gx_tev_ind[i]); off += 4; }
    for (u32 i = 0; i < 9; i++) { wr32be(p + off, gc_gx_ind_mtx[i]); off += 4; }
    wr32be(p + off, gc_gx_dirty_state); off += 4;
    wr32be(p + off, gc_gx_bp_sent_not); off += 4;
}
