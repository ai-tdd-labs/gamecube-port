#include <stdint.h>
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t  u8;
typedef int32_t  s32;
typedef int8_t   s8;

// Indirect texturing typedefs (matching sdk_port enums).
typedef s32 GXTevStageID;
typedef s32 GXIndTexStageID;
typedef s32 GXIndTexFormat;
typedef s32 GXIndTexBiasSel;
typedef s32 GXIndTexMtxID;
typedef s32 GXIndTexWrap;
typedef s32 GXIndTexAlphaSel;
typedef s32 GXIndTexScale;

// gc_mem init (needed for sdk_state RAM-backed store).
#include "src/sdk_port/gc_mem.h"
#include "src/sdk_port/sdk_state.h"

// SDK functions under test.
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

// State globals from GX.c.
extern u32 gc_gx_gen_mode;
extern u32 gc_gx_iref;
extern u32 gc_gx_ind_tex_scale0;
extern u32 gc_gx_ind_tex_scale1;
extern u32 gc_gx_tev_ind[16];
extern u32 gc_gx_ind_mtx[9];
extern u32 gc_gx_dirty_state;
extern u32 gc_gx_bp_sent_not;

static inline void wr32be(volatile uint8_t *p, u32 v) {
    p[0]=(uint8_t)(v>>24); p[1]=(uint8_t)(v>>16);
    p[2]=(uint8_t)(v>>8);  p[3]=(uint8_t)(v);
}

int main(void) {
    gc_mem_set(0x80000000u, 0x01800000u, (uint8_t *)0x80000000u);
    gc_sdk_state_reset();

    volatile uint8_t *out = (volatile uint8_t *)0x80300000;
    // Clear output area.
    for (u32 i = 0; i < 0x100; i++) out[i] = 0;

    // 1. GXSetNumIndStages(2) — sets genMode[16..18] = 2
    GXSetNumIndStages(2);

    // 2. GXSetIndTexOrder — stage 0: texmap=3, texcoord=5; stage 1: texmap=1, texcoord=2
    GXSetIndTexOrder(0, 5, 3);
    GXSetIndTexOrder(1, 2, 1);

    // 3. GXSetIndTexCoordScale — stage 0: scaleS=3, scaleT=5; stage 1: scaleS=2, scaleT=4
    GXSetIndTexCoordScale(0, 3, 5);
    GXSetIndTexCoordScale(1, 2, 4);

    // 4. GXSetIndTexMtx — matrix 0 (GX_ITM_0=1) with known values, scale_exp=3
    float mtx0[2][3] = { {0.5f, 0.25f, 0.0f}, {0.0f, 0.75f, 0.125f} };
    GXSetIndTexMtx(1, mtx0, 3);  // GX_ITM_0

    // 5. GXSetTevIndirect — stage 0: full params
    GXSetTevIndirect(0, 0, 0, 7, 1, 3, 2, 1, 0, 1);
    // stage=0, ind_stage=0, fmt=ITF_8, bias=ITB_STU(7), mtx=ITM_0(1),
    // wrap_s=ITW_64(3), wrap_t=ITW_128(2), add_prev=1, utc_lod=0, alpha=1

    // 6. GXSetTevDirect — stage 1
    GXSetTevDirect(1);

    // 7. GXSetTevIndWarp — stage 2: ind_stage=1, signed=1, replace=0, mtx=ITM_0(1)
    GXSetTevIndWarp(2, 1, 1, 0, 1);

    // 8. GXSetTevIndTile — stage 3: ind_stage=0, tilesize 64x32, spacing 128x64,
    //    fmt=0, mtx=ITM_1(2), bias=7(STU), alpha=0
    GXSetTevIndTile(3, 0, 64, 32, 128, 64, 0, 2, 7, 0);

    // Dump state to output area.
    u32 off = 0;
    wr32be(out + off, 0xDEADBEEFu); off += 4;  // marker

    // genMode (with ind stages bits).
    wr32be(out + off, gc_gx_gen_mode); off += 4;

    // iref
    wr32be(out + off, gc_gx_iref); off += 4;

    // IndTexScale0, IndTexScale1
    wr32be(out + off, gc_gx_ind_tex_scale0); off += 4;
    wr32be(out + off, gc_gx_ind_tex_scale1); off += 4;

    // tev_ind[0..3] (4 stages used)
    for (u32 i = 0; i < 4; i++) {
        wr32be(out + off, gc_gx_tev_ind[i]); off += 4;
    }

    // ind_mtx[0..8] (3 matrices * 3 regs)
    for (u32 i = 0; i < 9; i++) {
        wr32be(out + off, gc_gx_ind_mtx[i]); off += 4;
    }

    // dirty_state, bp_sent_not
    wr32be(out + off, gc_gx_dirty_state); off += 4;
    wr32be(out + off, gc_gx_bp_sent_not); off += 4;

    while (1) { __asm__ volatile("nop"); }
    return 0;
}
