#include <stdint.h>

typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;
typedef int32_t s32;
typedef int8_t s8;

typedef s32 GXTevStageID;
typedef s32 GXIndTexStageID;
typedef s32 GXIndTexFormat;
typedef s32 GXIndTexBiasSel;
typedef s32 GXIndTexMtxID;
typedef s32 GXIndTexWrap;
typedef s32 GXIndTexAlphaSel;

enum {
    GX_ITM_OFF   = 0,
    GX_ITM_0     = 1,
    GX_ITM_1     = 2,
    GX_ITM_2     = 3,
    GX_ITM_S0    = 5,
    GX_ITM_S1    = 6,
    GX_ITM_S2    = 7,
    GX_ITM_T0    = 9,
    GX_ITM_T1    = 10,
    GX_ITM_T2    = 11,
    GX_ITW_OFF   = 0,
    GX_ITW_256   = 1,
    GX_ITW_128   = 2,
    GX_ITW_64    = 3,
    GX_ITW_32    = 4,
    GX_ITW_16    = 5,
    GX_ITW_0     = 6,
};

u32 gc_gx_tev_ind[16];
u32 gc_gx_ind_mtx[9];
u32 gc_gx_bp_sent_not;

static inline u32 set_field(u32 reg, u32 width, u32 shift, u32 val) {
    u32 mask;
    if (width >= 32u) {
        mask = 0xFFFFFFFFu;
    } else {
        mask = ((1u << width) - 1u) << shift;
    }
    return (reg & ~mask) | ((val << shift) & mask);
}

void GXSetTevIndirect(GXTevStageID tev_stage, GXIndTexStageID ind_stage,
                      GXIndTexFormat format, GXIndTexBiasSel bias_sel,
                      GXIndTexMtxID matrix_sel,
                      GXIndTexWrap wrap_s, GXIndTexWrap wrap_t,
                      u8 add_prev, u8 utc_lod,
                      GXIndTexAlphaSel alpha_sel)
{
    u32 reg = 0u;
    reg = set_field(reg, 2u, 0u, (u32)ind_stage);
    reg = set_field(reg, 2u, 2u, (u32)format);
    reg = set_field(reg, 3u, 4u, (u32)bias_sel);
    reg = set_field(reg, 2u, 7u, (u32)alpha_sel);
    reg = set_field(reg, 4u, 9u, (u32)matrix_sel);
    reg = set_field(reg, 3u, 13u, (u32)wrap_s);
    reg = set_field(reg, 3u, 16u, (u32)wrap_t);
    reg = set_field(reg, 1u, 19u, (u32)utc_lod);
    reg = set_field(reg, 1u, 20u, (u32)add_prev);
    reg = set_field(reg, 8u, 24u, (u32)(tev_stage + 16));

    if ((u32)tev_stage < 16u) {
        gc_gx_tev_ind[tev_stage] = reg;
    }
    gc_gx_bp_sent_not = 0u;
}

void GXSetIndTexMtx(GXIndTexMtxID mtx_id, float offset[2][3], s8 scale_exp) {
    u32 id;
    switch (mtx_id) {
        case GX_ITM_0: case GX_ITM_1: case GX_ITM_2:
            id = (u32)(mtx_id - 1);
            break;
        case GX_ITM_S0: case GX_ITM_S1: case GX_ITM_S2:
            id = (u32)(mtx_id - 5);
            break;
        case GX_ITM_T0: case GX_ITM_T1: case GX_ITM_T2:
            id = (u32)(mtx_id - 9);
            break;
        default:
            id = 0u;
            break;
    }

    s32 m0 = (s32)(1024.0f * offset[0][0]) & 0x7FF;
    s32 m1 = (s32)(1024.0f * offset[1][0]) & 0x7FF;
    s8 se = scale_exp + 0x11;

    u32 reg0 = 0u;
    reg0 = set_field(reg0, 11u, 0u, (u32)m0);
    reg0 = set_field(reg0, 11u, 11u, (u32)m1);
    reg0 = set_field(reg0, 2u, 22u, (u32)(se & 3));
    reg0 = set_field(reg0, 8u, 24u, id * 3u + 6u);

    s32 m2 = (s32)(1024.0f * offset[0][1]) & 0x7FF;
    s32 m3 = (s32)(1024.0f * offset[1][1]) & 0x7FF;

    u32 reg1 = 0u;
    reg1 = set_field(reg1, 11u, 0u, (u32)m2);
    reg1 = set_field(reg1, 11u, 11u, (u32)m3);
    reg1 = set_field(reg1, 2u, 22u, (u32)((se >> 2) & 3));
    reg1 = set_field(reg1, 8u, 24u, id * 3u + 7u);

    s32 m4 = (s32)(1024.0f * offset[0][2]) & 0x7FF;
    s32 m5 = (s32)(1024.0f * offset[1][2]) & 0x7FF;

    u32 reg2 = 0u;
    reg2 = set_field(reg2, 11u, 0u, (u32)m4);
    reg2 = set_field(reg2, 11u, 11u, (u32)m5);
    reg2 = set_field(reg2, 2u, 22u, (u32)((se >> 4) & 3));
    reg2 = set_field(reg2, 8u, 24u, id * 3u + 8u);

    if (id < 3u) {
        gc_gx_ind_mtx[id * 3u + 0u] = reg0;
        gc_gx_ind_mtx[id * 3u + 1u] = reg1;
        gc_gx_ind_mtx[id * 3u + 2u] = reg2;
    }
    gc_gx_bp_sent_not = 0u;
}

void GXSetTevIndTile(GXTevStageID tev_stage, GXIndTexStageID ind_stage,
                     u16 tilesize_s, u16 tilesize_t,
                     u16 tilespacing_s, u16 tilespacing_t,
                     GXIndTexFormat format, GXIndTexMtxID matrix_sel,
                     GXIndTexBiasSel bias_sel, GXIndTexAlphaSel alpha_sel)
{
    GXIndTexWrap wrap_s, wrap_t;
    switch (tilesize_s) {
        case 256: wrap_s = GX_ITW_256; break;
        case 128: wrap_s = GX_ITW_128; break;
        case 64:  wrap_s = GX_ITW_64;  break;
        case 32:  wrap_s = GX_ITW_32;  break;
        case 16:  wrap_s = GX_ITW_16;  break;
        default:  wrap_s = GX_ITW_OFF; break;
    }
    switch (tilesize_t) {
        case 256: wrap_t = GX_ITW_256; break;
        case 128: wrap_t = GX_ITW_128; break;
        case 64:  wrap_t = GX_ITW_64;  break;
        case 32:  wrap_t = GX_ITW_32;  break;
        case 16:  wrap_t = GX_ITW_16;  break;
        default:  wrap_t = GX_ITW_OFF; break;
    }

    float mtx[2][3];
    mtx[0][0] = tilespacing_s / 1024.0f;
    mtx[0][1] = 0.0f;
    mtx[0][2] = 0.0f;
    mtx[1][0] = 0.0f;
    mtx[1][1] = tilespacing_t / 1024.0f;
    mtx[1][2] = 0.0f;

    GXSetIndTexMtx(matrix_sel, mtx, 0xA);
    GXSetTevIndirect(tev_stage, ind_stage, format, bias_sel, matrix_sel,
                     wrap_s, wrap_t, 0u, 1u, alpha_sel);
}
