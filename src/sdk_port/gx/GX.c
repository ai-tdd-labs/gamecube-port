#include <stdint.h>
#include <math.h>

typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;
typedef int16_t s16;
typedef int32_t s32;
typedef float f32;

typedef struct {
    u8 r;
    u8 g;
    u8 b;
    u8 a;
} GXColor;

// Light mask bits (GXEnum.h: GXLightID values). Kept here so GXLight helpers and
// GXSetChanCtrl share one definition.
enum {
    GX_LIGHT0 = 1u << 0,
    GX_LIGHT1 = 1u << 1,
    GX_LIGHT2 = 1u << 2,
    GX_LIGHT3 = 1u << 3,
    GX_LIGHT4 = 1u << 4,
    GX_LIGHT5 = 1u << 5,
    GX_LIGHT6 = 1u << 6,
    GX_LIGHT7 = 1u << 7,
};

// Public SDK declares this as u32 dummy[16] (64 bytes). We use the real layout from decomp.
typedef struct {
    u32 reserved[3];
    u32 Color;
    f32 a[3];
    f32 k[3];
    f32 lpos[3];
    f32 ldir[3];
} GXLightObj;

typedef enum {
    GX_SP_OFF,
    GX_SP_FLAT,
    GX_SP_COS,
    GX_SP_COS2,
    GX_SP_SHARP,
    GX_SP_RING1,
    GX_SP_RING2,
} GXSpotFn;

typedef enum {
    GX_DA_OFF,
    GX_DA_GENTLE,
    GX_DA_MEDIUM,
    GX_DA_STEEP,
} GXDistAttnFn;

// RAM-backed state (big-endian in MEM1) for dump comparability.
#include "../sdk_state.h"

// Minimal GX state mirror. We only model fields asserted by our deterministic tests.
u32 gc_gx_in_disp_list;
u32 gc_gx_dl_save_context;
u32 gc_gx_gen_mode;
u32 gc_gx_bp_mask;

u32 gc_gx_bp_sent_not;
u32 gc_gx_last_ras_reg;
float gc_gx_vp_left;
float gc_gx_vp_top;
float gc_gx_vp_wd;
float gc_gx_vp_ht;
float gc_gx_vp_nearz;
float gc_gx_vp_farz;

u32 gc_gx_su_scis0;
u32 gc_gx_su_scis1;
u32 gc_gx_su_ts0[8];
u32 gc_gx_lp_size;
u32 gc_gx_scissor_box_offset_reg;
u32 gc_gx_clip_mode;

// XF register mirror (only indices asserted by tests are meaningful).
// Size includes the projection block (XF regs 32..38).
u32 gc_gx_xf_regs[64];

// Light/channel color mirrors.
u32 gc_gx_amb_color[2];
u32 gc_gx_mat_color[2];

// Pixel engine control mirrors (subset).
u32 gc_gx_cmode0;
u32 gc_gx_pe_ctrl;
u32 gc_gx_zmode;

u32 gc_gx_cp_disp_src;
u32 gc_gx_cp_disp_size;
u32 gc_gx_cp_disp_stride;
u32 gc_gx_cp_disp;

// Texture copy registers (GXSetTexCopySrc/Dst + GXCopyTex).
u32 gc_gx_cp_tex_src;
u32 gc_gx_cp_tex_size;
u32 gc_gx_cp_tex_stride;
u32 gc_gx_cp_tex;
u32 gc_gx_cp_tex_z;
u32 gc_gx_cp_tex_addr_reg;
u32 gc_gx_cp_tex_written_reg;

// Additional state surfaced for tests that model MP4 callsites.
u32 gc_gx_copy_filter_aa;
u32 gc_gx_copy_filter_vf;
u32 gc_gx_copy_filter_sample_hash;
u32 gc_gx_copy_filter_vfilter_hash;
u32 gc_gx_pixel_fmt;
u32 gc_gx_z_fmt;
u32 gc_gx_copy_disp_dest;
u32 gc_gx_copy_disp_clear;
u32 gc_gx_copy_gamma;

u32 gc_gx_invalidate_vtx_cache_calls;
u32 gc_gx_invalidate_tex_all_calls;
u32 gc_gx_draw_done_calls;

// Copy-clear observable state (GXSetCopyClear writes 3 BP regs: 0x4F/0x50/0x51).
u32 gc_gx_copy_clear_reg0;
u32 gc_gx_copy_clear_reg1;
u32 gc_gx_copy_clear_reg2;

// Current matrix index observable (GXSetCurrentMtx updates matIdxA + XF reg 24).
u32 gc_gx_mat_idx_a;

// Draw-done shim (GXSetDrawDone/GXWaitDrawDone). We do not emulate PE finish interrupts,
// only the observable register writes and a deterministic completion flag for tests.
u32 gc_gx_set_draw_done_calls;
u32 gc_gx_wait_draw_done_calls;
u32 gc_gx_draw_done_flag;

// Fog observable BP regs (GXSetFog writes 0xEE..0xF2).
u32 gc_gx_fog0;
u32 gc_gx_fog1;
u32 gc_gx_fog2;
u32 gc_gx_fog3;
u32 gc_gx_fogclr;

// Projection observable state (GXSetProjection writes XF regs 32..38).
u32 gc_gx_proj_type;
u32 gc_gx_proj_mtx_bits[6];

// GX metric state (software mirror). Real hardware accumulates counters; for now we model
// only what our deterministic tests assert and what MP4 callsites depend on.
u32 gc_gx_gp_perf0;
u32 gc_gx_gp_perf1;
u32 gc_gx_vcache_sel;
u32 gc_gx_pix_metrics[6];
u32 gc_gx_mem_metrics[10];

u32 gc_gx_zmode_enable;
u32 gc_gx_zmode_func;
u32 gc_gx_zmode_update_enable;

u32 gc_gx_color_update_enable;

// Vertex descriptor/format state (GXAttr).
u32 gc_gx_vcd_lo;
u32 gc_gx_vcd_hi;
u32 gc_gx_has_nrms;
u32 gc_gx_has_binrms;
u32 gc_gx_nrm_type;
u32 gc_gx_dirty_state;
u32 gc_gx_dirty_vat;
u32 gc_gx_vat_a[8];
u32 gc_gx_vat_b[8];
u32 gc_gx_vat_c[8];

// Vertex array base/stride state (GXSetArray writes CP regs).
u32 gc_gx_array_base[32];
u32 gc_gx_array_stride[32];

// TEV state mirror (GXTev).
u32 gc_gx_tevc[16];
u32 gc_gx_teva[16];
u32 gc_gx_tref[8];
u32 gc_gx_texmap_id[16];
u32 gc_gx_tev_tc_enab;

// GXSetTevColor observable BP/RAS packed regs (last call).
u32 gc_gx_tev_color_reg_ra_last;
u32 gc_gx_tev_color_reg_bg_last;

// Immediate-mode vertex helpers (in the real SDK these write to the FIFO).
u32 gc_gx_pos3f32_x_bits;
u32 gc_gx_pos3f32_y_bits;
u32 gc_gx_pos3f32_z_bits;
u32 gc_gx_pos1x16_last;
u32 gc_gx_pos2s16_x;
u32 gc_gx_pos2s16_y;
u32 gc_gx_pos2u16_x;
u32 gc_gx_pos2u16_y;
u32 gc_gx_pos3s16_x;
u32 gc_gx_pos3s16_y;
u32 gc_gx_pos3s16_z;
u32 gc_gx_pos2f32_x_bits;
u32 gc_gx_pos2f32_y_bits;
u32 gc_gx_texcoord2f32_s_bits;
u32 gc_gx_texcoord2f32_t_bits;
u32 gc_gx_color1x8_last;
u32 gc_gx_color3u8_last;

// Token / draw sync state (GXManage).
uintptr_t gc_gx_token_cb_ptr;
u32 gc_gx_last_draw_sync_token;

// Minimal FIFO write mirror for deterministic transform tests.
// Real GX writes into the GP FIFO; we record the last command + payload words.
u32 gc_gx_fifo_u8_last;
u32 gc_gx_fifo_u32_last;
u32 gc_gx_fifo_mtx_words[12];

// Minimal GXBegin header mirror for deterministic geometry tests.
u32 gc_gx_fifo_begin_u8;
u32 gc_gx_fifo_begin_u16;

// Minimal texcoordgen XF state (GXSetTexCoordGen2 writes XF regs 0x40..0x47 and 0x50..0x57).
u32 gc_gx_xf_texcoordgen_40[8];
u32 gc_gx_xf_texcoordgen_50[8];

// Matrix index B mirror (GXTransform.c:__GXSetMatrixIndex uses matIdxA/matIdxB).
u32 gc_gx_mat_idx_b;

// Texture load observable state (GXLoadTexObjPreLoaded writes 6 BP regs).
u32 gc_gx_tex_load_mode0_last;
u32 gc_gx_tex_load_mode1_last;
u32 gc_gx_tex_load_image0_last;
u32 gc_gx_tex_load_image1_last;
u32 gc_gx_tex_load_image2_last;
u32 gc_gx_tex_load_image3_last;

// GXLight observable state (we mirror "last loaded" light object fields by index).
u32 gc_gx_light_loaded_mask;
GXLightObj gc_gx_light_loaded[8];

// Display list mirrors (GXDisplayList.c).
u32 gc_gx_dl_base;
u32 gc_gx_dl_size;
u32 gc_gx_dl_count;
u32 gc_gx_call_dl_list;
u32 gc_gx_call_dl_nbytes;

typedef struct {
    uint8_t _dummy;
} GXFifoObj;

static GXFifoObj s_fifo_obj;

// Helpers to match SDK bitfield packing macros.
static inline u32 set_field(u32 reg, u32 size, u32 shift, u32 v) {
    const u32 mask = ((1u << size) - 1u) << shift;
    return (reg & ~mask) | ((v << shift) & mask);
}

static inline void gx_write_ras_reg(u32 v) {
    // Deterministic mirror of "last written" BP/RAS register value.
    gc_gx_last_ras_reg = v;
}

typedef void (*GXDrawSyncCallback)(u16 token);

GXDrawSyncCallback GXSetDrawSyncCallback(GXDrawSyncCallback cb) {
    GXDrawSyncCallback old = (GXDrawSyncCallback)gc_gx_token_cb_ptr;
    // Real SDK wraps with interrupt disable/restore; we model only the observable end state.
    gc_gx_token_cb_ptr = (uintptr_t)cb;
    return old;
}

void GXSetDrawSync(u16 token) {
    // Mirror the two RAS writes the SDK does and record the token for tests/smoke.
    // The intermediate bitfield set is redundant since reg already contains token.
    u32 reg = ((u32)token) | 0x48000000u;
    gx_write_ras_reg(reg);
    gx_write_ras_reg(reg);
    gc_gx_last_draw_sync_token = token;
    // GXFlush() is a no-op in this host model; we only keep the bpSentNot mirror.
    gc_gx_bp_sent_not = 0;
}

// -----------------------------------------------------------------------------
// GXLight (SDK behavior, mirrored for deterministic testing)
//
// Reference: decomp_mario_party_4/src/dolphin/gx/GXLight.c
// -----------------------------------------------------------------------------

void GXInitLightAttn(GXLightObj *lt_obj, f32 a0, f32 a1, f32 a2, f32 k0, f32 k1, f32 k2) {
    GXLightObj *obj = lt_obj;
    obj->a[0] = a0;
    obj->a[1] = a1;
    obj->a[2] = a2;
    obj->k[0] = k0;
    obj->k[1] = k1;
    obj->k[2] = k2;
}

void GXInitLightAttnK(GXLightObj *lt_obj, f32 k0, f32 k1, f32 k2) {
    GXLightObj *obj = lt_obj;
    obj->k[0] = k0;
    obj->k[1] = k1;
    obj->k[2] = k2;
}

void GXInitLightSpot(GXLightObj *lt_obj, f32 cutoff, GXSpotFn spot_func) {
    f32 a0, a1, a2;
    f32 d;
    f32 cr;
    GXLightObj *obj = lt_obj;

    if (cutoff <= 0.0f || cutoff > 90.0f) {
        spot_func = GX_SP_OFF;
    }

    cr = cosf((3.1415927f * cutoff) / 180.0f);
    switch (spot_func) {
        case GX_SP_FLAT:
            a0 = -1000.0f * cr;
            a1 = 1000.0f;
            a2 = 0.0f;
            break;
        case GX_SP_COS:
            a0 = -cr / (1.0f - cr);
            a1 = 1.0f / (1.0f - cr);
            a2 = 0.0f;
            break;
        case GX_SP_COS2:
            a0 = 0.0f;
            a1 = -cr / (1.0f - cr);
            a2 = 1.0f / (1.0f - cr);
            break;
        case GX_SP_SHARP:
            d = (1.0f - cr) * (1.0f - cr);
            a0 = (cr * (cr - 2.0f)) / d;
            a1 = 2.0f / d;
            a2 = -1.0f / d;
            break;
        case GX_SP_RING1:
            d = (1.0f - cr) * (1.0f - cr);
            a0 = (-4.0f * cr) / d;
            a1 = (4.0f * (1.0f + cr)) / d;
            a2 = -4.0f / d;
            break;
        case GX_SP_RING2:
            d = (1.0f - cr) * (1.0f - cr);
            a0 = 1.0f - ((2.0f * cr * cr) / d);
            a1 = (4.0f * cr) / d;
            a2 = -2.0f / d;
            break;
        case GX_SP_OFF:
        default:
            a0 = 1.0f;
            a1 = 0.0f;
            a2 = 0.0f;
            break;
    }
    obj->a[0] = a0;
    obj->a[1] = a1;
    obj->a[2] = a2;
}

void GXInitLightDistAttn(GXLightObj *lt_obj, f32 ref_dist, f32 ref_br, GXDistAttnFn dist_func) {
    f32 k0, k1, k2;
    GXLightObj *obj = lt_obj;

    if (ref_dist < 0.0f) {
        dist_func = GX_DA_OFF;
    }
    if (ref_br <= 0.0f || ref_br >= 1.0f) {
        dist_func = GX_DA_OFF;
    }

    switch (dist_func) {
        case GX_DA_GENTLE:
            k0 = 1.0f;
            k1 = (1.0f - ref_br) / (ref_br * ref_dist);
            k2 = 0.0f;
            break;
        case GX_DA_MEDIUM:
            k0 = 1.0f;
            k1 = (0.5f * (1.0f - ref_br)) / (ref_br * ref_dist);
            k2 = (0.5f * (1.0f - ref_br)) / (ref_br * ref_dist * ref_dist);
            break;
        case GX_DA_STEEP:
            k0 = 1.0f;
            k1 = 0.0f;
            k2 = (1.0f - ref_br) / (ref_br * ref_dist * ref_dist);
            break;
        case GX_DA_OFF:
        default:
            k0 = 1.0f;
            k1 = 0.0f;
            k2 = 0.0f;
            break;
    }

    obj->k[0] = k0;
    obj->k[1] = k1;
    obj->k[2] = k2;
}

void GXInitLightPos(GXLightObj *lt_obj, f32 x, f32 y, f32 z) {
    GXLightObj *obj = lt_obj;
    obj->lpos[0] = x;
    obj->lpos[1] = y;
    obj->lpos[2] = z;
}

void GXInitLightDir(GXLightObj *lt_obj, f32 nx, f32 ny, f32 nz) {
    GXLightObj *obj = lt_obj;
    obj->ldir[0] = -nx;
    obj->ldir[1] = -ny;
    obj->ldir[2] = -nz;
}

void GXInitSpecularDir(GXLightObj *lt_obj, f32 nx, f32 ny, f32 nz) {
    f32 mag;
    f32 vx;
    f32 vy;
    f32 vz;
    GXLightObj *obj = lt_obj;

    vx = -nx;
    vy = -ny;
    vz = -nz + 1.0f;
    mag = 1.0f / sqrtf((vx * vx) + (vy * vy) + (vz * vz));
    obj->ldir[0] = vx * mag;
    obj->ldir[1] = vy * mag;
    obj->ldir[2] = vz * mag;
    obj->lpos[0] = -nx * 1048576.0f;
    obj->lpos[1] = -ny * 1048576.0f;
    obj->lpos[2] = -nz * 1048576.0f;
}

void GXInitLightColor(GXLightObj *lt_obj, GXColor color) {
    GXLightObj *obj = lt_obj;
    obj->Color = ((u32)color.r << 24) | ((u32)color.g << 16) | ((u32)color.b << 8) | (u32)color.a;
}

static u32 light_id_to_idx(u32 light) {
    switch (light) {
        case GX_LIGHT0: return 0;
        case GX_LIGHT1: return 1;
        case GX_LIGHT2: return 2;
        case GX_LIGHT3: return 3;
        case GX_LIGHT4: return 4;
        case GX_LIGHT5: return 5;
        case GX_LIGHT6: return 6;
        case GX_LIGHT7: return 7;
        default: return 0;
    }
}

void GXLoadLightObjImm(GXLightObj *lt_obj, u32 light) {
    u32 idx = light_id_to_idx(light);
    gc_gx_light_loaded[idx] = *lt_obj;
    gc_gx_light_loaded_mask |= (1u << idx);
    gc_gx_bp_sent_not = 1;
}

// -----------------------------------------------------------------------------
// GXDisplayList (minimal deterministic mirror)
//
// Reference: decomp_mario_party_4/src/dolphin/gx/GXDisplayList.c
// -----------------------------------------------------------------------------

void GXBeginDisplayList(void *list, u32 size) {
    // We do not model the real FIFO switching; we only mirror the observable mode.
    gc_gx_dl_base = (u32)(uintptr_t)list;
    gc_gx_dl_size = size;
    gc_gx_dl_count = 0;
    gc_gx_in_disp_list = 1;
}

u32 GXEndDisplayList(void) {
    // Real SDK validates overflow and returns byte count written. Our sdk_port does not
    // emit real FIFO bytes yet, so count remains 0 for now.
    u32 count = gc_gx_dl_count;
    gc_gx_in_disp_list = 0;
    return count;
}

void GXCallDisplayList(const void *list, u32 nbytes) {
    // Mirror the FIFO command payload (list pointer + byte count).
    gc_gx_call_dl_list = (u32)(uintptr_t)list;
    gc_gx_call_dl_nbytes = nbytes;
}

static inline void gx_write_xf_reg(u32 idx, u32 v) {
    if (idx < (sizeof(gc_gx_xf_regs) / sizeof(gc_gx_xf_regs[0]))) {
        gc_gx_xf_regs[idx] = v;
    }
}

// ---- Texture objects / regions (GXTexture.c + GXInit.c) ----
//
// These are placed before GXInit so we can initialize the default region pool.

// Internal GXTexObj layout (0x20 bytes) from decomp_mario_party_4/src/dolphin/gx/GXTexture.c.
// NOTE: Use 32-bit slots for pointers so host builds match the GameCube layout.
typedef struct {
    u32 mode0;
    u32 mode1;
    u32 image0;
    u32 image3;
    u32 userData;
    u32 fmt;
    u32 tlutName;
    u16 loadCnt;
    u8 loadFmt;
    u8 flags;
} GXTexObj;

// Internal GXTexRegion layout (matches __GXTexRegionInt in GXTexture.c).
typedef struct {
    u32 image1;
    u32 image2;
    u16 sizeEven;
    u16 sizeOdd;
    u8 is32bMipmap;
    u8 isCached;
    u8 _pad[2];
} GXTexRegion;

enum {
    GX_TEXCACHE_32K = 0,
    GX_TEXCACHE_128K = 1,
    GX_TEXCACHE_512K = 2,
    GX_TEXCACHE_NONE = 3,
};

// Default region pool installed by GXInit (GXInit.c).
static GXTexRegion gc_gx_tex_regions[8];
static u32 gc_gx_next_tex_rgn;

typedef GXTexRegion *(*GXTexRegionCallback)(GXTexObj *t_obj, u32 unused);
static GXTexRegionCallback gc_gx_tex_region_cb;

void GXInitTexCacheRegion(GXTexRegion *region, u8 is_32b_mipmap, u32 tmem_even, u32 size_even, u32 tmem_odd, u32 size_odd);
static GXTexRegion *gc__gx_default_tex_region_cb(GXTexObj *t_obj, u32 unused);

static const u8 gc_gx_tex_mode0_ids[8] = { 0x80, 0x81, 0x82, 0x83, 0xA0, 0xA1, 0xA2, 0xA3 };
static const u8 gc_gx_tex_mode1_ids[8] = { 0x84, 0x85, 0x86, 0x87, 0xA4, 0xA5, 0xA6, 0xA7 };
static const u8 gc_gx_tex_image0_ids[8] = { 0x88, 0x89, 0x8A, 0x8B, 0xA8, 0xA9, 0xAA, 0xAB };
static const u8 gc_gx_tex_image1_ids[8] = { 0x8C, 0x8D, 0x8E, 0x8F, 0xAC, 0xAD, 0xAE, 0xAF };
static const u8 gc_gx_tex_image2_ids[8] = { 0x90, 0x91, 0x92, 0x93, 0xB0, 0xB1, 0xB2, 0xB3 };
static const u8 gc_gx_tex_image3_ids[8] = { 0x94, 0x95, 0x96, 0x97, 0xB4, 0xB5, 0xB6, 0xB7 };

GXFifoObj *GXInit(void *base, u32 size) {
    (void)base;
    (void)size;
    gc_gx_in_disp_list = 0;
    gc_gx_dl_save_context = 1;
    gc_gx_gen_mode = 0;
    gc_gx_bp_mask = 0xFF;

    // Keep deterministic defaults for higher-level GX state tracked by our tests.
    gc_gx_bp_sent_not = 0;
    gc_gx_last_ras_reg = 0;
    gc_gx_vcd_lo = 0;
    gc_gx_vcd_hi = 0;
    gc_gx_has_nrms = 0;
    gc_gx_has_binrms = 0;
    gc_gx_nrm_type = 0;
    gc_gx_dirty_state = 0;
    gc_gx_dirty_vat = 0;
    u32 i;
    for (i = 0; i < 8; i++) {
        gc_gx_vat_a[i] = 0;
        gc_gx_vat_b[i] = 0;
        gc_gx_vat_c[i] = 0;
    }
    for (i = 0; i < 32; i++) {
        gc_gx_array_base[i] = 0;
        gc_gx_array_stride[i] = 0;
    }
    for (i = 0; i < 16; i++) {
        gc_gx_tevc[i] = 0;
        gc_gx_teva[i] = 0;
        gc_gx_texmap_id[i] = 0;
    }
    for (i = 0; i < 8; i++) {
        gc_gx_tref[i] = 0;
    }
    gc_gx_tev_tc_enab = 0;
    gc_gx_tev_color_reg_ra_last = 0;
    gc_gx_tev_color_reg_bg_last = 0;
    gc_gx_pos3f32_x_bits = 0;
    gc_gx_pos3f32_y_bits = 0;
    gc_gx_pos3f32_z_bits = 0;
    gc_gx_pos1x16_last = 0;
    gc_gx_fifo_u8_last = 0;
    gc_gx_fifo_u32_last = 0;
    for (i = 0; i < 12; i++) {
        gc_gx_fifo_mtx_words[i] = 0;
    }
    gc_gx_fifo_begin_u8 = 0;
    gc_gx_fifo_begin_u16 = 0;
    for (i = 0; i < 8; i++) {
        gc_gx_xf_texcoordgen_40[i] = 0;
        gc_gx_xf_texcoordgen_50[i] = 0;
    }
    gc_gx_mat_idx_b = 0;
    for (i = 0; i < 8; i++) {
        gc_gx_su_ts0[i] = 0;
    }
    gc_gx_lp_size = 0;
    gc_gx_scissor_box_offset_reg = 0;
    gc_gx_clip_mode = 0;
    for (i = 0; i < 32; i++) {
        gc_gx_xf_regs[i] = 0;
    }
    gc_gx_amb_color[0] = 0;
    gc_gx_amb_color[1] = 0;
    gc_gx_mat_color[0] = 0;
    gc_gx_mat_color[1] = 0;
    gc_gx_cmode0 = 0;
    gc_gx_pe_ctrl = 0;

    // Texture region defaults (GXInit.c).
    gc_gx_next_tex_rgn = 0;
    gc_gx_tex_region_cb = gc__gx_default_tex_region_cb;
    for (i = 0; i < 8; i++) {
        GXInitTexCacheRegion(&gc_gx_tex_regions[i], 0, i * 0x8000u, GX_TEXCACHE_32K, 0x80000u + i * 0x8000u, GX_TEXCACHE_32K);
    }
    gc_gx_tex_load_mode0_last = 0;
    gc_gx_tex_load_mode1_last = 0;
    gc_gx_tex_load_image0_last = 0;
    gc_gx_tex_load_image1_last = 0;
    gc_gx_tex_load_image2_last = 0;
    gc_gx_tex_load_image3_last = 0;

    return &s_fifo_obj;
}

typedef struct {
    u32 viTVmode;
    u16 fbWidth;
    u16 efbHeight;
    u16 xfbHeight;
    u16 viXOrigin;
    u16 viYOrigin;
    u16 viWidth;
    u16 viHeight;
    u32 xFBmode;
} GXRenderModeObj;

void GXAdjustForOverscan(GXRenderModeObj *rmin, GXRenderModeObj *rmout, u16 hor, u16 ver) {
    u16 hor2 = (u16)(hor * 2u);
    u16 ver2 = (u16)(ver * 2u);
    u32 verf;

    if (rmin != rmout) *rmout = *rmin;

    rmout->fbWidth = (u16)(rmin->fbWidth - hor2);
    verf = (u32)(ver2 * rmin->efbHeight) / (u32)rmin->xfbHeight;
    rmout->efbHeight = (u16)(rmin->efbHeight - verf);
    if (rmin->xFBmode == 1u && ((rmin->viTVmode & 2u) != 2u)) {
        rmout->xfbHeight = (u16)(rmin->xfbHeight - ver);
    } else {
        rmout->xfbHeight = (u16)(rmin->xfbHeight - ver2);
    }

    rmout->viWidth = (u16)(rmin->viWidth - hor2);
    rmout->viHeight = (u16)(rmin->viHeight - ver2);
    rmout->viXOrigin = (u16)(rmin->viXOrigin + hor);
    rmout->viYOrigin = (u16)(rmin->viYOrigin + ver);
}

void GXLoadPosMtxImm(float mtx[3][4], u32 id) {
    // Mirror decomp_mario_party_4/src/dolphin/gx/GXTransform.c:GXLoadPosMtxImm observable FIFO writes.
    const u32 addr = id * 4u;
    const u32 reg = addr | 0xB0000u;

    gc_gx_fifo_u8_last = 0x10u;
    gc_gx_fifo_u32_last = reg;

    u32 i = 0;
    u32 r, c;
    for (r = 0; r < 3; r++) {
        for (c = 0; c < 4; c++) {
            union {
                float f;
                u32 u;
            } u = { mtx[r][c] };
            gc_gx_fifo_mtx_words[i++] = u.u;
        }
    }
}

void GXLoadNrmMtxImm(float mtx[3][4], u32 id) {
    // Mirror decomp_mario_party_4/src/dolphin/gx/GXTransform.c:GXLoadNrmMtxImm observable FIFO writes.
    // Writes a 3x3 matrix derived from the top-left 3x3 of a 3x4 input.
    const u32 addr = id * 3u + 0x400u;
    const u32 reg = addr | 0x80000u;

    gc_gx_fifo_u8_last = 0x10u;
    gc_gx_fifo_u32_last = reg;

    u32 i = 0;
    u32 r, c;
    for (r = 0; r < 3; r++) {
        for (c = 0; c < 3; c++) {
            union {
                float f;
                u32 u;
            } u = { mtx[r][c] };
            gc_gx_fifo_mtx_words[i++] = u.u;
        }
    }
    for (; i < 12; i++) {
        gc_gx_fifo_mtx_words[i] = 0;
    }
}

void GXLoadTexMtxImm(float mtx[][4], u32 id, u32 type) {
    // Mirror decomp_mario_party_4/src/dolphin/gx/GXTransform.c:GXLoadTexMtxImm observable FIFO writes.
    // type: GX_MTX3x4 = 0, GX_MTX2x4 = 1 (GXEnum.h).
    // id: GX_TEXMTX0.., GX_PTTEXMTX0 = 64 (GXEnum.h).
    u32 addr;
    if (id >= 64u) {
        addr = (id - 64u) * 4u + 0x500u;
        // SDK asserts (type == GX_MTX3x4) for post-transform matrices.
    } else {
        addr = id * 4u;
    }

    const u32 count = (type == 1u) ? 8u : 12u;
    const u32 reg = addr | ((count - 1u) << 16);

    gc_gx_fifo_u8_last = 0x10u;
    gc_gx_fifo_u32_last = reg;

    u32 i = 0;
    u32 r, c;
    if (type == 0u) {
        // 3x4
        for (r = 0; r < 3; r++) {
            for (c = 0; c < 4; c++) {
                union {
                    float f;
                    u32 u;
                } u = { mtx[r][c] };
                gc_gx_fifo_mtx_words[i++] = u.u;
            }
        }
    } else {
        // 2x4
        for (r = 0; r < 2; r++) {
            for (c = 0; c < 4; c++) {
                union {
                    float f;
                    u32 u;
                } u = { mtx[r][c] };
                gc_gx_fifo_mtx_words[i++] = u.u;
            }
        }
    }
    for (; i < 12; i++) {
        gc_gx_fifo_mtx_words[i] = 0;
    }
}

void GXSetViewportJitter(float left, float top, float wd, float ht, float nearz, float farz, u32 field) {
    // We only model the software-visible state that our tests assert.
    if (field == 0) top -= 0.5f;
    gc_gx_vp_left = left;
    gc_gx_vp_top = top;
    gc_gx_vp_wd = wd;
    gc_gx_vp_ht = ht;
    gc_gx_vp_nearz = nearz;
    gc_gx_vp_farz = farz;
    gc_gx_bp_sent_not = 1;
}

void GXSetViewport(float left, float top, float wd, float ht, float nearz, float farz) {
    GXSetViewportJitter(left, top, wd, ht, nearz, farz, 1u);
}

void GXSetScissor(u32 left, u32 top, u32 wd, u32 ht) {
    u32 tp = top + 342u;
    u32 lf = left + 342u;
    u32 bm = tp + ht - 1u;
    u32 rt = lf + wd - 1u;

    // suScis0: tp (11 bits @ 0) | lf (11 bits @ 12)
    gc_gx_su_scis0 = set_field(gc_gx_su_scis0, 11, 0, tp);
    gc_gx_su_scis0 = set_field(gc_gx_su_scis0, 11, 12, lf);
    // suScis1: bm (11 bits @ 0) | rt (11 bits @ 12)
    gc_gx_su_scis1 = set_field(gc_gx_su_scis1, 11, 0, bm);
    gc_gx_su_scis1 = set_field(gc_gx_su_scis1, 11, 12, rt);

    gc_gx_bp_sent_not = 0;
}

void GXSetNumTexGens(u8 nTexGens) {
    // GXAttr.c: genMode[0..3] = nTexGens; dirtyState |= 4
    gc_gx_gen_mode = set_field(gc_gx_gen_mode, 4, 0, (u32)nTexGens);
    gc_gx_dirty_state |= 4u;
}

void GXClearVtxDesc(void) {
    // GXAttr.c: vcdLo=0; vcdLo[9..10]=1; vcdHi=0; hasNrms=0; hasBiNrms=0; dirtyState |= 8
    gc_gx_vcd_lo = 0;
    gc_gx_vcd_lo = set_field(gc_gx_vcd_lo, 2, 9, 1u);
    gc_gx_vcd_hi = 0;
    gc_gx_has_nrms = 0;
    gc_gx_has_binrms = 0;
    gc_gx_dirty_state |= 8u;
}

void GXSetLineWidth(u8 width, u32 texOffsets) {
    // GXGeometry.c: lpSize[0..7]=width, lpSize[16..18]=texOffsets; bpSentNot=0
    gc_gx_lp_size = set_field(gc_gx_lp_size, 8, 0, (u32)width);
    gc_gx_lp_size = set_field(gc_gx_lp_size, 3, 16, texOffsets & 0x7u);
    gc_gx_bp_sent_not = 0;
}

void GXSetPointSize(u8 pointSize, u32 texOffsets) {
    // GXGeometry.c: lpSize[8..15]=pointSize, lpSize[19..21]=texOffsets; bpSentNot=0
    gc_gx_lp_size = set_field(gc_gx_lp_size, 8, 8, (u32)pointSize);
    gc_gx_lp_size = set_field(gc_gx_lp_size, 3, 19, texOffsets & 0x7u);
    gc_gx_bp_sent_not = 0;
}

void GXEnableTexOffsets(u32 coord, u8 line_enable, u8 point_enable) {
    // GXGeometry.c: suTs0[coord].line=bit18, point=bit19; bpSentNot=0
    if (coord >= 8) return;
    gc_gx_su_ts0[coord] = set_field(gc_gx_su_ts0[coord], 1, 18, (u32)(line_enable != 0));
    gc_gx_su_ts0[coord] = set_field(gc_gx_su_ts0[coord], 1, 19, (u32)(point_enable != 0));
    gc_gx_bp_sent_not = 0;
}

void GXBegin(u8 type, u8 vtxfmt, u16 nverts) {
    // Mirror decomp_mario_party_4/src/dolphin/gx/GXGeometry.c:GXBegin observable FIFO header writes.
    gc_gx_fifo_begin_u8 = (u32)(vtxfmt | type);
    gc_gx_fifo_begin_u16 = (u32)nverts;
}

void GXEnd(void) {
    // In the SDK this is a macro barrier; deterministic host model: no-op.
}

void GXSetTexCoordGen2(u8 dst_coord, u32 func, u32 src_param, u32 mtx, u32 normalize, u32 postmtx) {
    // Mirror decomp_mario_party_4/src/dolphin/gx/GXAttr.c:GXSetTexCoordGen2.
    // For deterministic tests we model only the observable end state:
    // - XF reg 0x40+dst_coord and 0x50+dst_coord payloads
    // - matIdxA/matIdxB update and the XF reg 24/25 write via __GXSetMatrixIndex
    u32 reg = 0;
    u32 row = 5;
    u32 form = 0;

    if (dst_coord >= 8) return;

    switch (src_param) {
    case 0: /* GX_TG_POS */     row = 0; form = 1; break;
    case 1: /* GX_TG_NRM */     row = 1; form = 1; break;
    case 2: /* GX_TG_BINRM */   row = 3; form = 1; break;
    case 3: /* GX_TG_TANGENT */ row = 4; form = 1; break;
    case 4: /* GX_TG_TEX0 */    row = 5; break;
    case 5: /* GX_TG_TEX1 */    row = 6; break;
    case 6: /* GX_TG_TEX2 */    row = 7; break;
    case 7: /* GX_TG_TEX3 */    row = 8; break;
    case 8: /* GX_TG_TEX4 */    row = 9; break;
    case 9: /* GX_TG_TEX5 */    row = 10; break;
    case 10: /* GX_TG_TEX6 */   row = 11; break;
    case 11: /* GX_TG_TEX7 */   row = 12; break;
    case 19: /* GX_TG_COLOR0 */ row = 2; break;
    case 20: /* GX_TG_COLOR1 */ row = 2; break;
    default:
        // Extend when new callsites require more source parameters.
        row = 5;
        form = 0;
        break;
    }

    switch (func) {
    case 1: /* GX_TG_MTX2x4 */
        reg = set_field(reg, 1, 1, 0);
        reg = set_field(reg, 1, 2, form);
        reg = set_field(reg, 3, 4, 0);
        reg = set_field(reg, 5, 7, row);
        break;
    case 0: /* GX_TG_MTX3x4 */
        reg = set_field(reg, 1, 1, 1);
        reg = set_field(reg, 1, 2, form);
        reg = set_field(reg, 3, 4, 0);
        reg = set_field(reg, 5, 7, row);
        break;
    default:
        // Extend when new callsites require other gen types.
        break;
    }

    gc_gx_xf_texcoordgen_40[dst_coord] = reg;

    reg = 0;
    reg = set_field(reg, 6, 0, (postmtx - 64u) & 0x3Fu);
    reg = set_field(reg, 1, 8, (u32)(normalize != 0));
    gc_gx_xf_texcoordgen_50[dst_coord] = reg;

    // Matrix index update matches GXAttr.c switch over dst_coord.
    if (dst_coord <= 3u) {
        // matIdxA fields: TEX0..3 at shifts 6,12,18,24
        const u32 shift = 6u + dst_coord * 6u;
        gc_gx_mat_idx_a = set_field(gc_gx_mat_idx_a, 6, shift, mtx & 0x3Fu);
        gx_write_xf_reg(24, gc_gx_mat_idx_a);
    } else {
        // matIdxB fields: TEX4..7 at shifts 0,6,12,18
        const u32 shift = (dst_coord - 4u) * 6u;
        gc_gx_mat_idx_b = set_field(gc_gx_mat_idx_b, 6, shift, mtx & 0x3Fu);
        gx_write_xf_reg(25, gc_gx_mat_idx_b);
    }
    gc_gx_bp_sent_not = 1;
}

void GXSetTexCoordGen(u8 dst_coord, u32 func, u32 src_param, u32 mtx) {
    // Mirror inline wrapper in decomp_mario_party_4/include/dolphin/gx/GXGeometry.h.
    // normalize = GX_FALSE, postmtx = GX_PTIDENTITY (125).
    GXSetTexCoordGen2(dst_coord, func, src_param, mtx, 0, 125u);
}

void GXSetCoPlanar(u32 enable) {
    // GXGeometry.c: genMode[19] = enable; writes RAS regs
    gc_gx_gen_mode = set_field(gc_gx_gen_mode, 1, 19, (u32)(enable != 0));
}

void GXSetCullMode(u32 mode) {
    // GXGeometry.c: front/back swap for hwMode, then genMode[14..15] = hwMode; dirtyState |= 4
    u32 hwMode = mode;
    // These values match GXEnum: GX_CULL_FRONT=1, GX_CULL_BACK=2.
    if (mode == 1u) hwMode = 2u;
    else if (mode == 2u) hwMode = 1u;
    gc_gx_gen_mode = set_field(gc_gx_gen_mode, 2, 14, hwMode & 3u);
    gc_gx_dirty_state |= 4u;
}

void GXSetScissorBoxOffset(int32_t x_off, int32_t y_off) {
    // GXTransform.c: hx=(x_off+342)>>1, hy=(y_off+342)>>1, pack into reg 0x59, bpSentNot=0
    u32 reg = 0;
    u32 hx = (u32)(x_off + 342) >> 1;
    u32 hy = (u32)(y_off + 342) >> 1;
    reg = set_field(reg, 10, 0, hx);
    reg = set_field(reg, 10, 10, hy);
    reg = set_field(reg, 8, 24, 0x59u);
    gc_gx_scissor_box_offset_reg = reg;
    gc_gx_bp_sent_not = 0;
}

void GXSetClipMode(u32 mode) {
    // GXTransform.c: write XF reg 5; bpSentNot = 1
    gc_gx_clip_mode = mode;
    gc_gx_bp_sent_not = 1;
}

void GXSetNumChans(u8 nChans) {
    // GXLight.c: genMode[4..6]=nChans; dirtyState |= 4
    gc_gx_gen_mode = set_field(gc_gx_gen_mode, 3, 4, (u32)nChans);
    gc_gx_dirty_state |= 4u;
}

void GXSetDispCopySrc(u16 left, u16 top, u16 wd, u16 ht) {
    u32 reg = 0;
    reg = set_field(reg, 10, 0, left);
    reg = set_field(reg, 10, 10, top);
    reg = set_field(reg, 8, 24, 0x49);
    gc_sdk_state_store_u32_mirror(GC_SDK_OFF_GX_CP_DISP_SRC, &gc_gx_cp_disp_src, reg);

    reg = 0;
    reg = set_field(reg, 10, 0, (u32)(wd - 1u));
    reg = set_field(reg, 10, 10, (u32)(ht - 1u));
    reg = set_field(reg, 8, 24, 0x4A);
    gc_sdk_state_store_u32_mirror(GC_SDK_OFF_GX_CP_DISP_SIZE, &gc_gx_cp_disp_size, reg);

    // Mirror gx->cpDispSize usage by GXSetDispCopyYScale.
    gc_gx_bp_sent_not = 0;
}

void GXSetDispCopyDst(u16 wd, u16 ht) {
    (void)ht;
    u16 stride = (u16)(wd * 2u);
    u32 reg = 0;
    reg = set_field(reg, 10, 0, (u32)(stride >> 5));
    reg = set_field(reg, 8, 24, 0x4D);
    gc_gx_cp_disp_stride = reg;
    gc_gx_bp_sent_not = 0;
}

static u32 __GXGetNumXfbLines(u32 efbHt, u32 iScale) {
    u32 count = (efbHt - 1u) * 0x100u;
    u32 realHt = (count / iScale) + 1u;
    u32 iScaleD = iScale;

    if (iScaleD > 0x80u && iScaleD < 0x100u) {
        while ((iScaleD % 2u) == 0u) iScaleD /= 2u;
        if ((efbHt % iScaleD) == 0u) realHt++;
    }
    if (realHt > 0x400u) realHt = 0x400u;
    return realHt;
}

u32 GXSetDispCopyYScale(float vscale) {
    u32 scale = ((u32)(256.0f / vscale)) & 0x1FFu;
    u32 check = (scale != 0x100u);

    // Track cpDisp bit 10 (check) so tests can observe it.
    gc_gx_cp_disp = set_field(gc_gx_cp_disp, 1, 10, check);

    // Height is derived from cpDispSize bits [10..19] + 1. Our mirror uses the same packing.
    u32 height = ((gc_gx_cp_disp_size >> 10) & 0x3FFu) + 1u;
    gc_gx_bp_sent_not = 0;
    return __GXGetNumXfbLines(height, scale);
}

// From SDK: GXGetYScaleFactor(efbHeight, xfbHeight) -> fScale.
float GXGetYScaleFactor(u16 efbHeight, u16 xfbHeight) {
    u32 tgtHt = xfbHeight;
    float yScale = (float)xfbHeight / (float)efbHeight;
    u32 iScale = ((u32)(256.0f / yScale)) & 0x1FFu;
    u32 realHt = __GXGetNumXfbLines(efbHeight, iScale);

    while (realHt > xfbHeight) {
        tgtHt--;
        yScale = (float)tgtHt / (float)efbHeight;
        iScale = ((u32)(256.0f / yScale)) & 0x1FFu;
        realHt = __GXGetNumXfbLines(efbHeight, iScale);
    }

    float fScale = yScale;
    while (realHt < xfbHeight) {
        fScale = yScale;
        tgtHt++;
        yScale = (float)tgtHt / (float)efbHeight;
        iScale = ((u32)(256.0f / yScale)) & 0x1FFu;
        realHt = __GXGetNumXfbLines(efbHeight, iScale);
    }

    return fScale;
}

static u32 hash_bytes(const void *p, u32 n) {
    const u8 *b = (const u8 *)p;
    // Simple, stable checksum (not crypto): FNV-1a 32-bit.
    u32 h = 2166136261u;
    for (u32 i = 0; i < n; i++) {
        h ^= (u32)b[i];
        h *= 16777619u;
    }
    return h;
}

void GXSetCopyFilter(u8 aa, const u8 sample_pattern[12][2], u8 vf, const u8 vfilter[7]) {
    gc_gx_copy_filter_aa = (u32)aa;
    gc_gx_copy_filter_vf = (u32)vf;
    gc_gx_copy_filter_sample_hash = hash_bytes(sample_pattern, 12u * 2u);
    gc_gx_copy_filter_vfilter_hash = hash_bytes(vfilter, 7u);
}

void GXSetPixelFmt(u32 pix_fmt, u32 z_fmt) {
    gc_gx_pixel_fmt = pix_fmt;
    gc_gx_z_fmt = z_fmt;
}

void GXCopyDisp(void *dest, u8 clear) {
    gc_sdk_state_store_u32_mirror(GC_SDK_OFF_GX_COPY_DISP_DEST, &gc_gx_copy_disp_dest, (u32)(uintptr_t)dest);
    gc_gx_copy_disp_clear = (u32)clear;
}

void GXSetDispCopyGamma(u32 gamma) {
    gc_gx_copy_gamma = gamma;
}

// ---- GXInit tail setters (used by MP4 init chain after GXSetDither) ----

u32 gc_gx_dst_alpha_enable;
u32 gc_gx_dst_alpha;

u32 gc_gx_field_mask_even;
u32 gc_gx_field_mask_odd;

u32 gc_gx_field_mode_field_mode;
u32 gc_gx_field_mode_half_aspect;

u32 gc_gx_copy_clamp;
u32 gc_gx_copy_frame2field;

u32 gc_gx_clear_bounding_box_calls;

u32 gc_gx_poke_color_update_enable;
u32 gc_gx_poke_alpha_update_enable;
u32 gc_gx_poke_dither_enable;

u32 gc_gx_poke_blend_type;
u32 gc_gx_poke_blend_src;
u32 gc_gx_poke_blend_dst;
u32 gc_gx_poke_blend_op;

u32 gc_gx_poke_alpha_mode_func;
u32 gc_gx_poke_alpha_mode_thresh;
u32 gc_gx_poke_alpha_read_mode;

u32 gc_gx_poke_dst_alpha_enable;
u32 gc_gx_poke_dst_alpha;

u32 gc_gx_poke_zmode_enable;
u32 gc_gx_poke_zmode_func;
u32 gc_gx_poke_zmode_update_enable;

void GXSetDstAlpha(u8 enable, u8 alpha) {
    gc_gx_dst_alpha_enable = (u32)enable;
    gc_gx_dst_alpha = (u32)alpha;
}

void GXSetFieldMask(u8 even, u8 odd) {
    gc_gx_field_mask_even = (u32)even;
    gc_gx_field_mask_odd = (u32)odd;
}

void GXSetFieldMode(u8 field_mode, u8 half_aspect) {
    gc_gx_field_mode_field_mode = (u32)field_mode;
    gc_gx_field_mode_half_aspect = (u32)half_aspect;
}

void GXSetCopyClamp(u32 clamp) {
    gc_gx_copy_clamp = clamp;
}

void GXSetDispCopyFrame2Field(u32 mode) {
    gc_gx_copy_frame2field = mode;
}

void GXClearBoundingBox(void) {
    gc_gx_clear_bounding_box_calls++;
}

void GXPokeColorUpdate(u8 enable) {
    gc_gx_poke_color_update_enable = (u32)enable;
}

void GXPokeAlphaUpdate(u8 enable) {
    gc_gx_poke_alpha_update_enable = (u32)enable;
}

void GXPokeDither(u8 enable) {
    gc_gx_poke_dither_enable = (u32)enable;
}

void GXPokeBlendMode(u32 type, u32 src_factor, u32 dst_factor, u32 op) {
    gc_gx_poke_blend_type = type;
    gc_gx_poke_blend_src = src_factor;
    gc_gx_poke_blend_dst = dst_factor;
    gc_gx_poke_blend_op = op;
}

void GXPokeAlphaMode(u32 func, u8 threshold) {
    gc_gx_poke_alpha_mode_func = func;
    gc_gx_poke_alpha_mode_thresh = (u32)threshold;
}

void GXPokeAlphaRead(u32 mode) {
    gc_gx_poke_alpha_read_mode = mode;
}

void GXPokeDstAlpha(u8 enable, u8 alpha) {
    gc_gx_poke_dst_alpha_enable = (u32)enable;
    gc_gx_poke_dst_alpha = (u32)alpha;
}

void GXPokeZMode(u8 enable, u32 func, u8 update_enable) {
    gc_gx_poke_zmode_enable = (u32)enable;
    gc_gx_poke_zmode_func = func;
    gc_gx_poke_zmode_update_enable = (u32)update_enable;
}

void GXInvalidateVtxCache(void) {
    gc_gx_invalidate_vtx_cache_calls++;
}

void GXInvalidateTexAll(void) {
    gc_gx_invalidate_tex_all_calls++;
}

void GXDrawDone(void) {
    gc_gx_draw_done_calls++;
}

void GXSetTexCopySrc(u16 left, u16 top, u16 wd, u16 ht) {
    // Mirror GXFrameBuf.c:GXSetTexCopySrc (observable packed regs).
    // Note: wd/ht are stored as (wd-1)/(ht-1).
    gc_gx_cp_tex_src = 0;
    gc_gx_cp_tex_src = set_field(gc_gx_cp_tex_src, 10, 0, (u32)left);
    gc_gx_cp_tex_src = set_field(gc_gx_cp_tex_src, 10, 10, (u32)top);
    gc_gx_cp_tex_src = set_field(gc_gx_cp_tex_src, 8, 24, 0x49u);

    gc_gx_cp_tex_size = 0;
    gc_gx_cp_tex_size = set_field(gc_gx_cp_tex_size, 10, 0, (u32)(wd - 1u));
    gc_gx_cp_tex_size = set_field(gc_gx_cp_tex_size, 10, 10, (u32)(ht - 1u));
    gc_gx_cp_tex_size = set_field(gc_gx_cp_tex_size, 8, 24, 0x4Au);
}

enum {
    GX_TF_I4 = 0x0,
    GX_TF_I8 = 0x1,
    GX_TF_IA4 = 0x2,
    GX_TF_IA8 = 0x3,
    GX_TF_RGB565 = 0x4,
    GX_TF_RGB5A3 = 0x5,
    GX_TF_RGBA8 = 0x6,
    GX_TF_C4 = 0x8,
    GX_TF_C8 = 0x9,
    GX_TF_C14X2 = 0xA,
    GX_TF_CMPR = 0xE,
    GX_TF_Z16 = 0xF, // special cased in SDK
};

enum {
    // Copy texture formats (GX_CTF_*). MP4 uses GX_CTF_R8 for shadow.
    GX_CTF_R4 = 0x0,
    GX_CTF_RA4 = 0x2,
    GX_CTF_RA8 = 0x3,
    GX_CTF_R8 = 0x4,
    GX_CTF_G8 = 0x5,
    GX_CTF_B8 = 0x6,
    GX_CTF_RG8 = 0x7,
    GX_CTF_GB8 = 0x8,
    GX_CTF_Z8M = 0x9,
    GX_CTF_Z8L = 0xA,
    GX_CTF_Z16L = 0xC,
    GX_CTF_Z16M = 0xD,
};

static inline u32 gx_some_set_reg_macro(u32 reg, u32 val) {
    // Mirror GXTexture.c SOME_SET_REG_MACRO (rlwinm(...,0,27,23) clears bits 24..26).
    return (reg & 0xF8FFFFFFu) | val;
}

static GXTexRegion *gc__gx_default_tex_region_cb(GXTexObj *t_obj, u32 unused) {
    // Mirror decomp_mario_party_4/src/dolphin/gx/GXInit.c:__GXDefaultTexRegionCallback.
    (void)unused;
    // Non-CI formats (anything except C4/C8/C14X2) round-robin over 8 regions.
    if (t_obj && t_obj->fmt != 0x8u && t_obj->fmt != 0x9u && t_obj->fmt != 0xAu) {
        return &gc_gx_tex_regions[gc_gx_next_tex_rgn++ & 7u];
    }
    // CI path not needed yet; keep deterministic.
    return &gc_gx_tex_regions[0];
}

void GXInitTexObj(GXTexObj *obj, void *image_ptr, u16 width, u16 height, u32 format, u32 wrap_s, u32 wrap_t, u8 mipmap) {
    // Mirror decomp_mario_party_4/src/dolphin/gx/GXTexture.c:GXInitTexObj (observable packing only).
    GXTexObj *t = obj;
    if (!t) return;

    // Equivalent to memset(t, 0, 0x20) without relying on libc in this TU.
    {
        u32 *w = (u32 *)(void *)t;
        for (u32 i = 0; i < 8; i++) w[i] = 0;
    }

    t->mode0 = set_field(t->mode0, 2, 0, wrap_s);
    t->mode0 = set_field(t->mode0, 2, 2, wrap_t);
    t->mode0 = set_field(t->mode0, 1, 4, 1u);

    if (mipmap != 0) {
        // Extend when callsites require mipmap.
        t->flags |= 1u;
        // The SDK sets additional fields based on format and max lod.
        // For now keep the macro behavior deterministic and local to callsites.
        t->mode0 = gx_some_set_reg_macro(t->mode0, 0xC0u);
    } else {
        t->mode0 = gx_some_set_reg_macro(t->mode0, 0x80u);
    }

    t->fmt = format;

    t->image0 = set_field(t->image0, 10, 0, (u32)(width - 1u));
    t->image0 = set_field(t->image0, 10, 10, (u32)(height - 1u));
    t->image0 = set_field(t->image0, 4, 20, (u32)(format & 0xFu));

    {
        u32 imageBase = ((u32)((uintptr_t)image_ptr) >> 5) & 0x01FFFFFFu;
        t->image3 = set_field(t->image3, 21, 0, imageBase);
    }

    // Compute loadFmt + loadCnt based on format low nibble.
    u16 rowT = 2;
    u16 colT = 2;
    switch (format & 0xFu) {
    case 0: /* I4 */
    case 8: /* C4 */
        t->loadFmt = 1;
        rowT = 3;
        colT = 3;
        break;
    case 1: /* I8 */
    case 2: /* IA4 */
    case 9: /* C8 */
        t->loadFmt = 2;
        rowT = 3;
        colT = 2;
        break;
    case 3: /* IA8 */
    case 4: /* RGB565 */
    case 5: /* RGB5A3 */
    case 10: /* C14X2 */
        t->loadFmt = 2;
        rowT = 2;
        colT = 2;
        break;
    case 6: /* RGBA8 */
        t->loadFmt = 3;
        rowT = 2;
        colT = 2;
        break;
    case 14: /* CMPR */
        t->loadFmt = 0;
        rowT = 3;
        colT = 3;
        break;
    default:
        t->loadFmt = 2;
        rowT = 2;
        colT = 2;
        break;
    }

    {
        u32 rowC = ((u32)width + (1u << rowT) - 1u) >> rowT;
        u32 colC = ((u32)height + (1u << colT) - 1u) >> colT;
        t->loadCnt = (u16)((rowC * colC) & 0x7FFFu);
    }
    t->flags |= 2u;
}

void GXInitTexObjLOD(GXTexObj *obj, u32 min_filt, u32 mag_filt, float min_lod, float max_lod, float lod_bias, u8 bias_clamp, u8 do_edge_lod, u32 max_aniso) {
    // Mirror decomp_mario_party_4/src/dolphin/gx/GXTexture.c:GXInitTexObjLOD (observable packing only).
    // This mutates the texobj's mode0/mode1 fields in-place.
    static const u8 GX2HWFiltConv[6] = { 0x00, 0x04, 0x01, 0x05, 0x02, 0x06 };

    if (!obj) return;

    // Clamp lod_bias to [-4, 3.99] and pack as u8(32*bias) into mode0[9..16].
    if (lod_bias < -4.0f) lod_bias = -4.0f;
    else if (lod_bias >= 4.0f) lod_bias = 3.99f;
    u8 lbias = (u8)(32.0f * lod_bias);
    obj->mode0 = set_field(obj->mode0, 8, 9, (u32)lbias);

    // Filters: mag_filt==GX_LINEAR sets bit4, min_filt maps through GX2HWFiltConv into bits 5..7.
    obj->mode0 = set_field(obj->mode0, 1, 4, (mag_filt == 1u) ? 1u : 0u);
    if (min_filt > 5u) min_filt = 0u; // keep deterministic if caller passes junk
    obj->mode0 = set_field(obj->mode0, 3, 5, (u32)GX2HWFiltConv[min_filt]);

    // do_edge_lod is inverted in mode0 bit8 (SDK writes 0 when true, 1 when false).
    obj->mode0 = set_field(obj->mode0, 1, 8, do_edge_lod ? 0u : 1u);

    // Fixed fields asserted by decomp are always zeroed here.
    obj->mode0 = set_field(obj->mode0, 1, 17, 0u);
    obj->mode0 = set_field(obj->mode0, 1, 18, 0u);

    // Anisotropy into bits 19..20, bias_clamp into bit21.
    obj->mode0 = set_field(obj->mode0, 2, 19, max_aniso & 3u);
    obj->mode0 = set_field(obj->mode0, 1, 21, bias_clamp ? 1u : 0u);

    // Clamp min/max lod to [0,10] and pack as u8(16*x) into mode1[0..7] and [8..15].
    if (min_lod < 0.0f) min_lod = 0.0f;
    else if (min_lod > 10.0f) min_lod = 10.0f;
    u8 lmin = (u8)(16.0f * min_lod);

    if (max_lod < 0.0f) max_lod = 0.0f;
    else if (max_lod > 10.0f) max_lod = 10.0f;
    u8 lmax = (u8)(16.0f * max_lod);

    obj->mode1 = set_field(obj->mode1, 8, 0, (u32)lmin);
    obj->mode1 = set_field(obj->mode1, 8, 8, (u32)lmax);
}

void GXInitTexCacheRegion(GXTexRegion *region, u8 is_32b_mipmap, u32 tmem_even, u32 size_even, u32 tmem_odd, u32 size_odd) {
    // Mirror GXTexture.c:GXInitTexCacheRegion observable packing of image1/image2.
    if (!region) return;

    u32 WidthExp2;
    switch (size_even) {
    case GX_TEXCACHE_32K: WidthExp2 = 3; break;
    case GX_TEXCACHE_128K: WidthExp2 = 4; break;
    case GX_TEXCACHE_512K: WidthExp2 = 5; break;
    default: WidthExp2 = 3; break;
    }

    region->image1 = 0;
    region->image1 = set_field(region->image1, 15, 0, tmem_even >> 5);
    region->image1 = set_field(region->image1, 3, 15, WidthExp2);
    region->image1 = set_field(region->image1, 3, 18, WidthExp2);
    region->image1 = set_field(region->image1, 1, 21, 0);

    switch (size_odd) {
    case GX_TEXCACHE_32K: WidthExp2 = 3; break;
    case GX_TEXCACHE_128K: WidthExp2 = 4; break;
    case GX_TEXCACHE_512K: WidthExp2 = 5; break;
    case GX_TEXCACHE_NONE: WidthExp2 = 0; break;
    default: WidthExp2 = 0; break;
    }

    region->image2 = 0;
    region->image2 = set_field(region->image2, 15, 0, tmem_odd >> 5);
    region->image2 = set_field(region->image2, 3, 15, WidthExp2);
    region->image2 = set_field(region->image2, 3, 18, WidthExp2);
    region->is32bMipmap = is_32b_mipmap;
    region->isCached = 1;
}

void GXLoadTexObjPreLoaded(GXTexObj *obj, GXTexRegion *region, u32 id) {
    // Mirror GXTexture.c:GXLoadTexObjPreLoaded (observable BP regs only).
    if (!obj || !region) return;
    if (id >= 8u) return;

    obj->mode0 = set_field(obj->mode0, 8, 24, gc_gx_tex_mode0_ids[id]);
    obj->mode1 = set_field(obj->mode1, 8, 24, gc_gx_tex_mode1_ids[id]);
    obj->image0 = set_field(obj->image0, 8, 24, gc_gx_tex_image0_ids[id]);
    region->image1 = set_field(region->image1, 8, 24, gc_gx_tex_image1_ids[id]);
    region->image2 = set_field(region->image2, 8, 24, gc_gx_tex_image2_ids[id]);
    obj->image3 = set_field(obj->image3, 8, 24, gc_gx_tex_image3_ids[id]);

    // Deterministic observable output for tests.
    gc_gx_tex_load_mode0_last = obj->mode0;
    gc_gx_tex_load_mode1_last = obj->mode1;
    gc_gx_tex_load_image0_last = obj->image0;
    gc_gx_tex_load_image1_last = region->image1;
    gc_gx_tex_load_image2_last = region->image2;
    gc_gx_tex_load_image3_last = obj->image3;

    gx_write_ras_reg(obj->mode0);
    gx_write_ras_reg(obj->mode1);
    gx_write_ras_reg(obj->image0);
    gx_write_ras_reg(region->image1);
    gx_write_ras_reg(region->image2);
    gx_write_ras_reg(obj->image3);
}

void GXLoadTexObj(GXTexObj *obj, u32 id) {
    // Mirror GXTexture.c:GXLoadTexObj: select region via callback then preload.
    if (!gc_gx_tex_region_cb) gc_gx_tex_region_cb = gc__gx_default_tex_region_cb;
    GXTexRegion *r = gc_gx_tex_region_cb(obj, id);
    GXLoadTexObjPreLoaded(obj, r, id);
}

static void get_image_tile_count(u32 fmt, u16 wd, u16 ht, u32 *rowTiles, u32 *colTiles, u32 *cmpTiles) {
    // Minimal tile math to support MP4 GX_CTF_R8 callsite.
    // For other formats, extend as needed (evidence-driven).
    u16 tileW = 8;
    u16 tileH = 4;
    u32 cmp = 1;

    switch (fmt) {
    case GX_CTF_R8:
        tileW = 8;
        tileH = 4;
        cmp = 1;
        break;
    default:
        // Safe fallback: treat as 8x4 tiles.
        tileW = 8;
        tileH = 4;
        cmp = 1;
        break;
    }

    *rowTiles = (wd + tileW - 1u) / tileW;
    *colTiles = (ht + tileH - 1u) / tileH;
    *cmpTiles = cmp;
}

void GXSetTexCopyDst(u16 wd, u16 ht, u32 fmt, u32 mipmap) {
    // Mirror GXFrameBuf.c:GXSetTexCopyDst observable behavior for MP4 callsite (GX_CTF_R8).
    u32 rowTiles, colTiles, cmpTiles;
    u32 peTexFmt = fmt & 0xFu;
    u32 peTexFmtH;

    // gx->cpTexZ = 0;
    gc_gx_cp_tex_z = 0;

    // gx->cpTex field 15 depends on fmt class; for GX_CTF_R8 it goes to "default" (2).
    gc_gx_cp_tex = set_field(gc_gx_cp_tex, 2, 15, 2u);

    // Z texture flag.
    // _GX_TF_ZTF is 0x10 in SDK headers. We don't see MP4 use it here.
    gc_gx_cp_tex_z = (fmt & 0x10u) == 0x10u;

    peTexFmtH = (peTexFmt >> 3) & 1u;
    gc_gx_cp_tex = set_field(gc_gx_cp_tex, 1, 3, peTexFmtH);
    peTexFmt = peTexFmt & 7u;

    get_image_tile_count(fmt, wd, ht, &rowTiles, &colTiles, &cmpTiles);

    gc_gx_cp_tex_stride = 0;
    gc_gx_cp_tex_stride = set_field(gc_gx_cp_tex_stride, 10, 0, rowTiles * cmpTiles);
    gc_gx_cp_tex_stride = set_field(gc_gx_cp_tex_stride, 8, 24, 0x4Du);

    gc_gx_cp_tex = set_field(gc_gx_cp_tex, 1, 9, (u32)(mipmap != 0));
    gc_gx_cp_tex = set_field(gc_gx_cp_tex, 3, 4, peTexFmt);
}

void GXCopyTex(void *dest, u32 clear) {
    // Mirror GXFrameBuf.c:GXCopyTex observable packing/writes for deterministic tests.
    // We track the packed address reg (0x4B) and the cpTex reg write (0x52).
    u32 reg;
    u32 phyAddr = ((u32)(uintptr_t)dest) & 0x3FFFFFFFu;

    // Address BP reg 0x4B with phyAddr>>5 in low 21 bits.
    reg = 0;
    reg = set_field(reg, 21, 0, (phyAddr >> 5));
    reg = set_field(reg, 8, 24, 0x4Bu);
    gc_gx_cp_tex_addr_reg = reg;

    // cpTex write (0x52) sets clear bit11, and sets bit14=0 (tex copy).
    gc_gx_cp_tex = set_field(gc_gx_cp_tex, 1, 11, (u32)(clear != 0));
    gc_gx_cp_tex = set_field(gc_gx_cp_tex, 1, 14, 0u);
    gc_gx_cp_tex = set_field(gc_gx_cp_tex, 8, 24, 0x52u);
    gc_gx_cp_tex_written_reg = gc_gx_cp_tex;

    // Observable write ordering in the SDK includes cpTexSrc/cpTexSize/cpTexStride first.
    // For our deterministic oracle, we preserve those regs and update "last RAS reg" to
    // the last written register in the sequence.
    gx_write_ras_reg(gc_gx_cp_tex_src);
    gx_write_ras_reg(gc_gx_cp_tex_size);
    gx_write_ras_reg(gc_gx_cp_tex_stride);
    gx_write_ras_reg(gc_gx_cp_tex_addr_reg);
    gx_write_ras_reg(gc_gx_cp_tex_written_reg);

    if (clear != 0) {
        gx_write_ras_reg(gc_gx_zmode);
        // For the clear path, the SDK writes a locally modified cmode0:
        // bits0/1 are cleared (alpha update + dst alpha), reg id 0x42.
        u32 cmode_clear = gc_gx_cmode0;
        cmode_clear = set_field(cmode_clear, 1, 0, 0u);
        cmode_clear = set_field(cmode_clear, 1, 1, 0u);
        cmode_clear = set_field(cmode_clear, 8, 24, 0x42u);
        gx_write_ras_reg(cmode_clear);
    }

    gc_gx_bp_sent_not = 0;
}

/* Mirror decomp __GXGetTexTileShift — full tile shift table for all GX formats.
 * Format values use SDK encoding: base | _GX_TF_ZTF(0x10) | _GX_TF_CTF(0x20). */
static void __GXGetTexTileShift(u32 fmt, u32 *rowTileS, u32 *colTileS) {
    switch (fmt) {
    case 0x0:   /* GX_TF_I4 */
    case 0x8:   /* GX_TF_C4 */
    case 0xE:   /* GX_TF_CMPR */
    case 0x20:  /* GX_CTF_R4 */
    case 0x30:  /* GX_CTF_Z4 */
        *rowTileS = 3; *colTileS = 3; break;
    case 0x1:   /* GX_TF_I8 */
    case 0x2:   /* GX_TF_IA4 */
    case 0x9:   /* GX_TF_C8 */
    case 0x11:  /* GX_TF_Z8 */
    case 0x22:  /* GX_CTF_RA4 */
    case 0x27:  /* GX_TF_A8 */
    case 0x28:  /* GX_CTF_R8 */
    case 0x29:  /* GX_CTF_G8 */
    case 0x2A:  /* GX_CTF_B8 */
    case 0x39:  /* GX_CTF_Z8M */
    case 0x3A:  /* GX_CTF_Z8L */
        *rowTileS = 3; *colTileS = 2; break;
    case 0x3:   /* GX_TF_IA8 */
    case 0x4:   /* GX_TF_RGB565 */
    case 0x5:   /* GX_TF_RGB5A3 */
    case 0x6:   /* GX_TF_RGBA8 */
    case 0xA:   /* GX_TF_C14X2 */
    case 0x13:  /* GX_TF_Z16 */
    case 0x16:  /* GX_TF_Z24X8 */
    case 0x23:  /* GX_CTF_RA8 */
    case 0x2B:  /* GX_CTF_RG8 */
    case 0x2C:  /* GX_CTF_GB8 */
    case 0x3C:  /* GX_CTF_Z16L */
        *rowTileS = 2; *colTileS = 2; break;
    default:
        *rowTileS = *colTileS = 0; break;
    }
}

u32 GXGetTexBufferSize(u16 width, u16 height, u32 format, u8 mipmap, u8 max_lod) {
    // Mirror decomp_mario_party_4/src/dolphin/gx/GXTexture.c:GXGetTexBufferSize.
    u32 tileShiftX, tileShiftY;
    u32 tileBytes;
    u32 bufferSize;

    __GXGetTexTileShift(format, &tileShiftX, &tileShiftY);
    tileBytes = (format == 0x6u || format == 0x16u) ? 64u : 32u; /* RGBA8 or Z24X8 */

    if (mipmap == 1) {
        u32 level;
        bufferSize = 0;
        for (level = 0; level < (u32)max_lod; level++) {
            u32 nx = (width + (1u << tileShiftX) - 1u) >> tileShiftX;
            u32 ny = (height + (1u << tileShiftY) - 1u) >> tileShiftY;
            bufferSize += tileBytes * (nx * ny);
            if (width == 1 && height == 1) break;
            width = (width > 1) ? (u16)(width >> 1) : 1;
            height = (height > 1) ? (u16)(height >> 1) : 1;
        }
        return bufferSize;
    }

    {
        u32 nx = (width + (1u << tileShiftX) - 1u) >> tileShiftX;
        u32 ny = (height + (1u << tileShiftY) - 1u) >> tileShiftY;
        bufferSize = nx * ny * tileBytes;
    }
    return bufferSize;
}

void GXSetCopyClear(GXColor clear_clr, u32 clear_z) {
    // Mirror Dolphin SDK GXFrameBuf.c:GXSetCopyClear observable BP reg writes.
    // We track all 3 regs so callsite tests can validate the full sequence.
    u32 reg;
    reg = 0;
    reg = set_field(reg, 8, 0, (u32)clear_clr.r);
    reg = set_field(reg, 8, 8, (u32)clear_clr.a);
    reg = set_field(reg, 8, 24, 0x4Fu);
    gc_gx_copy_clear_reg0 = reg;
    gx_write_ras_reg(reg);

    reg = 0;
    reg = set_field(reg, 8, 0, (u32)clear_clr.b);
    reg = set_field(reg, 8, 8, (u32)clear_clr.g);
    reg = set_field(reg, 8, 24, 0x50u);
    gc_gx_copy_clear_reg1 = reg;
    gx_write_ras_reg(reg);

    reg = 0;
    reg = set_field(reg, 24, 0, clear_z & 0xFFFFFFu);
    reg = set_field(reg, 8, 24, 0x51u);
    gc_gx_copy_clear_reg2 = reg;
    gx_write_ras_reg(reg);

    gc_gx_bp_sent_not = 0;
}

void GXSetCurrentMtx(u32 id) {
    // Mirror GXTransform.c:GXSetCurrentMtx + __GXSetMatrixIndex(GX_VA_PNMTXIDX).
    // We model only the matIdxA update and the XF reg write (24).
    gc_gx_mat_idx_a = set_field(gc_gx_mat_idx_a, 6, 0, id);
    gx_write_xf_reg(24, gc_gx_mat_idx_a);
    gc_gx_bp_sent_not = 1;
}

void GXSetDrawDone(void) {
    // Mirror GXMisc.c:GXSetDrawDone observable write.
    // Real SDK also touches interrupt state + DrawDone flag; we keep it deterministic.
    gc_gx_set_draw_done_calls++;
    gx_write_ras_reg(0x45000002u);
    gc_gx_draw_done_flag = 0;
}

void GXWaitDrawDone(void) {
    // Deterministic completion: don't block, just mark "done".
    gc_gx_wait_draw_done_calls++;
    gc_gx_draw_done_flag = 1;
}

typedef float f32;
typedef enum {
    GX_FOG_NONE = 0,
} GXFogType;

void GXSetFog(GXFogType type, f32 startz, f32 endz, f32 nearz, f32 farz, GXColor color) {
    // Mirror decomp_mario_party_4/src/dolphin/gx/GXPixel.c:GXSetFog.
    // NOTE: For MP4 Hu3DFogClear callsite we hit the degenerate branch (all zeros),
    // which avoids platform FP quirks while still validating bitfield packing.
    u32 fogclr, fog0, fog1, fog2, fog3;
    f32 A, B, B_mant, C;
    f32 a, c;
    u32 B_expn;
    u32 b_m;
    u32 b_s;
    u32 a_hex;
    u32 c_hex;

    if (farz == nearz || endz == startz) {
        A = 0.0f;
        B = 0.5f;
        C = 0.0f;
    } else {
        A = (farz * nearz) / ((farz - nearz) * (endz - startz));
        B = farz / (farz - nearz);
        C = startz / (endz - startz);
    }

    B_mant = B;
    B_expn = 0;
    while (B_mant > 1.0f) {
        B_mant *= 0.5f;
        B_expn++;
    }
    while (B_mant > 0.0f && B_mant < 0.5f) {
        B_mant *= 2.0f;
        B_expn--;
    }

    a = A / (f32)(1u << (B_expn + 1u));
    b_m = (u32)(8.388638e6f * B_mant);
    b_s = B_expn + 1u;
    c = C;

    fog1 = 0;
    fog1 = set_field(fog1, 24, 0, b_m);
    fog1 = set_field(fog1, 8, 24, 0xEFu);

    fog2 = 0;
    fog2 = set_field(fog2, 5, 0, b_s);
    fog2 = set_field(fog2, 8, 24, 0xF0u);

    // Strict-aliasing safe float->bits.
    __builtin_memcpy(&a_hex, &a, sizeof(a_hex));
    __builtin_memcpy(&c_hex, &c, sizeof(c_hex));

    fog0 = 0;
    fog0 = set_field(fog0, 11, 0, (a_hex >> 12) & 0x7FFu);
    fog0 = set_field(fog0, 8, 11, (a_hex >> 23) & 0xFFu);
    fog0 = set_field(fog0, 1, 19, (a_hex >> 31));
    fog0 = set_field(fog0, 8, 24, 0xEEu);

    fog3 = 0;
    fog3 = set_field(fog3, 11, 0, (c_hex >> 12) & 0x7FFu);
    fog3 = set_field(fog3, 8, 11, (c_hex >> 23) & 0xFFu);
    fog3 = set_field(fog3, 1, 19, (c_hex >> 31));
    fog3 = set_field(fog3, 1, 20, 0);
    fog3 = set_field(fog3, 3, 21, (u32)type);
    fog3 = set_field(fog3, 8, 24, 0xF1u);

    fogclr = 0;
    fogclr = set_field(fogclr, 8, 0, (u32)color.b);
    fogclr = set_field(fogclr, 8, 8, (u32)color.g);
    fogclr = set_field(fogclr, 8, 16, (u32)color.r);
    fogclr = set_field(fogclr, 8, 24, 0xF2u);

    gc_gx_fog0 = fog0;
    gc_gx_fog1 = fog1;
    gc_gx_fog2 = fog2;
    gc_gx_fog3 = fog3;
    gc_gx_fogclr = fogclr;

    gx_write_ras_reg(fog0);
    gx_write_ras_reg(fog1);
    gx_write_ras_reg(fog2);
    gx_write_ras_reg(fog3);
    gx_write_ras_reg(fogclr);
    gc_gx_bp_sent_not = 0;
}

typedef enum {
    GX_PERSPECTIVE = 0,
    GX_ORTHOGRAPHIC = 1,
} GXProjectionType;

static void gx_set_projection_internal(void) {
    // Mirror decomp_mario_party_4/src/dolphin/gx/GXTransform.c:__GXSetProjection
    // observable XF writes (32..38). We don't model the FIFO header writes here.
    gx_write_xf_reg(32, gc_gx_proj_mtx_bits[0]);
    gx_write_xf_reg(33, gc_gx_proj_mtx_bits[1]);
    gx_write_xf_reg(34, gc_gx_proj_mtx_bits[2]);
    gx_write_xf_reg(35, gc_gx_proj_mtx_bits[3]);
    gx_write_xf_reg(36, gc_gx_proj_mtx_bits[4]);
    gx_write_xf_reg(37, gc_gx_proj_mtx_bits[5]);
    gx_write_xf_reg(38, gc_gx_proj_type);
}

void GXSetProjection(f32 mtx[4][4], GXProjectionType type) {
    // Mirror GXTransform.c:GXSetProjection packing and observable XF writes.
    gc_gx_proj_type = (u32)type;

    f32 p0 = mtx[0][0];
    f32 p2 = mtx[1][1];
    f32 p4 = mtx[2][2];
    f32 p5 = mtx[2][3];
    f32 p1;
    f32 p3;
    if (type == GX_ORTHOGRAPHIC) {
        p1 = mtx[0][3];
        p3 = mtx[1][3];
    } else {
        p1 = mtx[0][2];
        p3 = mtx[1][2];
    }

    __builtin_memcpy(&gc_gx_proj_mtx_bits[0], &p0, sizeof(u32));
    __builtin_memcpy(&gc_gx_proj_mtx_bits[1], &p1, sizeof(u32));
    __builtin_memcpy(&gc_gx_proj_mtx_bits[2], &p2, sizeof(u32));
    __builtin_memcpy(&gc_gx_proj_mtx_bits[3], &p3, sizeof(u32));
    __builtin_memcpy(&gc_gx_proj_mtx_bits[4], &p4, sizeof(u32));
    __builtin_memcpy(&gc_gx_proj_mtx_bits[5], &p5, sizeof(u32));

    gx_set_projection_internal();
    gc_gx_bp_sent_not = 1;
}

void GXGetProjectionv(f32 *ptr) {
    if (!ptr) {
        return;
    }

    ptr[0] = (f32)gc_gx_proj_type;
    __builtin_memcpy(&ptr[1], &gc_gx_proj_mtx_bits[0], sizeof(u32));
    __builtin_memcpy(&ptr[2], &gc_gx_proj_mtx_bits[1], sizeof(u32));
    __builtin_memcpy(&ptr[3], &gc_gx_proj_mtx_bits[2], sizeof(u32));
    __builtin_memcpy(&ptr[4], &gc_gx_proj_mtx_bits[3], sizeof(u32));
    __builtin_memcpy(&ptr[5], &gc_gx_proj_mtx_bits[4], sizeof(u32));
    __builtin_memcpy(&ptr[6], &gc_gx_proj_mtx_bits[5], sizeof(u32));
}

void GXSetZMode(u8 enable, u32 func, u8 update_enable) {
    gc_gx_zmode_enable = (u32)enable;
    gc_gx_zmode_func = func;
    gc_gx_zmode_update_enable = (u32)update_enable;

    // Mirror GXPixel.c: gx->zmode packed reg (only fields used by GXCopyTex clear path).
    // Layout: bit0 enable, bits1..3 func, bit4 update enable, high byte 0x40.
    gc_gx_zmode = 0;
    gc_gx_zmode = set_field(gc_gx_zmode, 1, 0, (u32)enable);
    gc_gx_zmode = set_field(gc_gx_zmode, 3, 1, func & 7u);
    gc_gx_zmode = set_field(gc_gx_zmode, 1, 4, (u32)update_enable);
    gc_gx_zmode = set_field(gc_gx_zmode, 8, 24, 0x40u);
}

void GXSetColorUpdate(u8 enable) {
    gc_gx_color_update_enable = (u32)enable;
    // Mirror GXPixel.c: cmode0 bit 3.
    gc_gx_cmode0 = set_field(gc_gx_cmode0, 1, 3, (u32)enable);
    gx_write_ras_reg(gc_gx_cmode0);
    gc_gx_bp_sent_not = 0;
}

// ---- Batch 3 (GX light/tev/pixel setters used by GXInit) ----

enum {
    GX_COLOR0 = 0,
    GX_COLOR1 = 1,
    GX_ALPHA0 = 2,
    GX_ALPHA1 = 3,
    GX_COLOR0A0 = 4,
    GX_COLOR1A1 = 5,
};

void GXSetChanCtrl(u32 chan, u8 enable, u32 amb_src, u32 mat_src, u32 light_mask, u32 diff_fn, u32 attn_fn) {
    u32 reg;
    u32 idx;

    if (chan == GX_COLOR0A0) idx = 0;
    else if (chan == GX_COLOR1A1) idx = 1;
    else idx = chan;

    reg = 0;
    reg = set_field(reg, 1, 1, (u32)(enable != 0));
    reg = set_field(reg, 1, 0, (u32)(mat_src != 0));
    reg = set_field(reg, 1, 6, (u32)(amb_src != 0));
    reg = set_field(reg, 1, 2, (light_mask & GX_LIGHT0) ? 1u : 0u);
    reg = set_field(reg, 1, 3, (light_mask & GX_LIGHT1) ? 1u : 0u);
    reg = set_field(reg, 1, 4, (light_mask & GX_LIGHT2) ? 1u : 0u);
    reg = set_field(reg, 1, 5, (light_mask & GX_LIGHT3) ? 1u : 0u);
    reg = set_field(reg, 1, 11, (light_mask & GX_LIGHT4) ? 1u : 0u);
    reg = set_field(reg, 1, 12, (light_mask & GX_LIGHT5) ? 1u : 0u);
    reg = set_field(reg, 1, 13, (light_mask & GX_LIGHT6) ? 1u : 0u);
    reg = set_field(reg, 1, 14, (light_mask & GX_LIGHT7) ? 1u : 0u);
    reg = set_field(reg, 2, 7, (attn_fn == 0) ? 0u : (diff_fn & 3u));
    reg = set_field(reg, 1, 9, (attn_fn != 2) ? 1u : 0u);
    reg = set_field(reg, 1, 10, (attn_fn != 0) ? 1u : 0u);

    gx_write_xf_reg(idx + 14, reg);
    gc_gx_bp_sent_not = 1;
    if (chan == GX_COLOR0A0) {
        gx_write_xf_reg(16, reg);
    } else if (chan == GX_COLOR1A1) {
        gx_write_xf_reg(17, reg);
    }
}

void GXSetChanAmbColor(u32 chan, GXColor amb_color) {
    u32 reg = 0;
    u32 colIdx = 0;
    u32 alpha;

    switch (chan) {
    case GX_COLOR0:
        alpha = gc_gx_amb_color[0] & 0xFFu;
        reg = set_field(reg, 8, 0, alpha);
        reg = set_field(reg, 8, 8, amb_color.b);
        reg = set_field(reg, 8, 16, amb_color.g);
        reg = set_field(reg, 8, 24, amb_color.r);
        colIdx = 0;
        break;
    case GX_COLOR1:
        alpha = gc_gx_amb_color[1] & 0xFFu;
        reg = set_field(reg, 8, 0, alpha);
        reg = set_field(reg, 8, 8, amb_color.b);
        reg = set_field(reg, 8, 16, amb_color.g);
        reg = set_field(reg, 8, 24, amb_color.r);
        colIdx = 1;
        break;
    case GX_ALPHA0:
        reg = gc_gx_amb_color[0];
        reg = set_field(reg, 8, 0, amb_color.a);
        colIdx = 0;
        break;
    case GX_ALPHA1:
        reg = gc_gx_amb_color[1];
        reg = set_field(reg, 8, 0, amb_color.a);
        colIdx = 1;
        break;
    case GX_COLOR0A0:
        reg = set_field(reg, 8, 0, amb_color.a);
        reg = set_field(reg, 8, 8, amb_color.b);
        reg = set_field(reg, 8, 16, amb_color.g);
        reg = set_field(reg, 8, 24, amb_color.r);
        colIdx = 0;
        break;
    case GX_COLOR1A1:
        reg = set_field(reg, 8, 0, amb_color.a);
        reg = set_field(reg, 8, 8, amb_color.b);
        reg = set_field(reg, 8, 16, amb_color.g);
        reg = set_field(reg, 8, 24, amb_color.r);
        colIdx = 1;
        break;
    default:
        return;
    }

    gx_write_xf_reg(colIdx + 10, reg);
    gc_gx_bp_sent_not = 1;
    gc_gx_amb_color[colIdx] = reg;
}

void GXSetChanMatColor(u32 chan, GXColor mat_color) {
    u32 reg = 0;
    u32 colIdx = 0;
    u32 alpha;

    switch (chan) {
    case GX_COLOR0:
        alpha = gc_gx_mat_color[0] & 0xFFu;
        reg = set_field(reg, 8, 0, alpha);
        reg = set_field(reg, 8, 8, mat_color.b);
        reg = set_field(reg, 8, 16, mat_color.g);
        reg = set_field(reg, 8, 24, mat_color.r);
        colIdx = 0;
        break;
    case GX_COLOR1:
        alpha = gc_gx_mat_color[1] & 0xFFu;
        reg = set_field(reg, 8, 0, alpha);
        reg = set_field(reg, 8, 8, mat_color.b);
        reg = set_field(reg, 8, 16, mat_color.g);
        reg = set_field(reg, 8, 24, mat_color.r);
        colIdx = 1;
        break;
    case GX_ALPHA0:
        reg = gc_gx_mat_color[0];
        reg = set_field(reg, 8, 0, mat_color.a);
        colIdx = 0;
        break;
    case GX_ALPHA1:
        reg = gc_gx_mat_color[1];
        reg = set_field(reg, 8, 0, mat_color.a);
        colIdx = 1;
        break;
    case GX_COLOR0A0:
        reg = set_field(reg, 8, 0, mat_color.a);
        reg = set_field(reg, 8, 8, mat_color.b);
        reg = set_field(reg, 8, 16, mat_color.g);
        reg = set_field(reg, 8, 24, mat_color.r);
        colIdx = 0;
        break;
    case GX_COLOR1A1:
        reg = set_field(reg, 8, 0, mat_color.a);
        reg = set_field(reg, 8, 8, mat_color.b);
        reg = set_field(reg, 8, 16, mat_color.g);
        reg = set_field(reg, 8, 24, mat_color.r);
        colIdx = 1;
        break;
    default:
        return;
    }

    gx_write_xf_reg(colIdx + 12, reg);
    gc_gx_bp_sent_not = 1;
    gc_gx_mat_color[colIdx] = reg;
}

void GXSetNumTevStages(u8 nStages) {
    // GXTev.c: genMode[10..13] = nStages-1; dirtyState |= 4
    if (nStages == 0 || nStages > 16) return;
    gc_gx_gen_mode = set_field(gc_gx_gen_mode, 4, 10, (u32)(nStages - 1u));
    gc_gx_dirty_state |= 4u;
}

void GXSetNumIndStages(u8 nIndStages) {
    // GXBump.c: genMode bits 16..18 hold indirect stage count (0..4).
    gc_gx_gen_mode = set_field(gc_gx_gen_mode, 3, 16, (u32)(nIndStages & 7u));
    gc_gx_dirty_state |= 6u;
}

// Forward declarations (implemented later in this file).
void GXSetTevColorIn(u32 stage, u32 a, u32 b, u32 c, u32 d);
void GXSetTevAlphaIn(u32 stage, u32 a, u32 b, u32 c, u32 d);
void GXSetTevColorOp(u32 stage, u32 op, u32 bias, u32 scale, u32 clamp, u32 out_reg);
void GXSetTevAlphaOp(u32 stage, u32 op, u32 bias, u32 scale, u32 clamp, u32 out_reg);

void GXSetTevOp(u32 id, u32 mode) {
    // Mirror GXTev.c behavior for the modes our tests exercise.
    u32 carg = 10u; // GX_CC_RASC
    u32 aarg = 5u;  // GX_CA_RASA
    if (id != 0) {
        carg = 12u; // GX_CC_CPREV
        aarg = 6u;  // GX_CA_APREV
    }

    switch (mode) {
    case 0: // GX_MODULATE
        GXSetTevColorIn(id, 0u, 8u, carg, 0u);
        GXSetTevAlphaIn(id, 0u, 4u, aarg, 0u);
        break;
    case 1: // GX_DECAL
        GXSetTevColorIn(id, carg, 8u, 9u, 0u);
        GXSetTevAlphaIn(id, 0u, 0u, 0u, aarg);
        break;
    case 2: // GX_BLEND
        GXSetTevColorIn(id, carg, 1u, 8u, 0u);
        GXSetTevAlphaIn(id, 0u, 4u, aarg, 0u);
        break;
    case 3: // GX_REPLACE
        GXSetTevColorIn(id, 0u, 0u, 0u, 8u);
        GXSetTevAlphaIn(id, 0u, 0u, 0u, 4u);
        break;
    case 4: // GX_PASSCLR
        GXSetTevColorIn(id, 0u, 0u, 0u, carg);
        GXSetTevAlphaIn(id, 0u, 0u, 0u, aarg);
        break;
    default:
        break;
    }

    // GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, clamp=true, out=GX_TEVPREV
    GXSetTevColorOp(id, 0u, 0u, 0u, 1u, 0u);
    GXSetTevAlphaOp(id, 0u, 0u, 0u, 1u, 0u);
}

void GXSetAlphaCompare(u32 comp0, u8 ref0, u32 op, u32 comp1, u8 ref1) {
    // GXTev.c: pack into a RAS reg with high byte 0xF3.
    u32 reg = 0;
    reg = set_field(reg, 8, 0, (u32)ref0);
    reg = set_field(reg, 8, 8, (u32)ref1);
    reg = set_field(reg, 3, 16, comp0 & 7u);
    reg = set_field(reg, 3, 19, comp1 & 7u);
    reg = set_field(reg, 2, 22, op & 3u);
    reg = set_field(reg, 8, 24, 0xF3u);
    gx_write_ras_reg(reg);
    gc_gx_bp_sent_not = 0;
}

void GXSetBlendMode(u32 type, u32 src_factor, u32 dst_factor, u32 op) {
    // GXPixel.c: writes cmode0 with high byte 0x41.
    const u32 blend_enable = (type == 1u || type == 3u) ? 1u : 0u; // BLEND or SUBTRACT
    const u32 subtract = (type == 3u) ? 1u : 0u;
    const u32 logic = (type == 2u) ? 1u : 0u;

    gc_gx_cmode0 = set_field(gc_gx_cmode0, 1, 0, blend_enable);
    gc_gx_cmode0 = set_field(gc_gx_cmode0, 1, 11, subtract);
    gc_gx_cmode0 = set_field(gc_gx_cmode0, 1, 1, logic);
    gc_gx_cmode0 = set_field(gc_gx_cmode0, 4, 12, op & 0xFu);
    gc_gx_cmode0 = set_field(gc_gx_cmode0, 3, 8, src_factor & 7u);
    gc_gx_cmode0 = set_field(gc_gx_cmode0, 3, 5, dst_factor & 7u);
    gc_gx_cmode0 = set_field(gc_gx_cmode0, 8, 24, 0x41u);
    gx_write_ras_reg(gc_gx_cmode0);
    gc_gx_bp_sent_not = 0;
}

void GXSetAlphaUpdate(u8 update_enable) {
    // GXPixel.c: cmode0 bit 4.
    gc_gx_cmode0 = set_field(gc_gx_cmode0, 1, 4, (u32)(update_enable != 0));
    gx_write_ras_reg(gc_gx_cmode0);
    gc_gx_bp_sent_not = 0;
}

void GXSetZCompLoc(u8 before_tex) {
    // GXPixel.c: peCtrl bit 6.
    gc_gx_pe_ctrl = set_field(gc_gx_pe_ctrl, 1, 6, (u32)(before_tex != 0));
    gx_write_ras_reg(gc_gx_pe_ctrl);
    gc_gx_bp_sent_not = 0;
}

void GXPixModeSync(void) {
    // GXMisc.c: write peCtrl BP register and clear bpSentNot.
    gx_write_ras_reg(gc_gx_pe_ctrl);
    gc_gx_bp_sent_not = 0;
}

void GXResetWriteGatherPipe(void) {
    // GXMisc.c touches PPC WPAR hardware state; no modeled memory-visible side effects.
    // Keep as a host-safe no-op.
}

void GXSetDither(u8 dither) {
    // GXPixel.c: cmode0 bit 2.
    gc_gx_cmode0 = set_field(gc_gx_cmode0, 1, 2, (u32)(dither != 0));
    gx_write_ras_reg(gc_gx_cmode0);
    gc_gx_bp_sent_not = 0;
}

void GXSetGPMetric(u32 perf0, u32 perf1) {
    gc_gx_gp_perf0 = perf0;
    gc_gx_gp_perf1 = perf1;
}

void GXClearGPMetric(void) {
    gc_gx_gp_perf0 = 0;
    gc_gx_gp_perf1 = 0;
}

void GXReadGPMetric(u32 *met0, u32 *met1) {
    if (met0) *met0 = gc_gx_gp_perf0;
    if (met1) *met1 = gc_gx_gp_perf1;
}

void GXSetVCacheMetric(u32 attr) {
    gc_gx_vcache_sel = attr;
}

void GXClearVCacheMetric(void) {
    gc_gx_vcache_sel = 0;
}

void GXReadVCacheMetric(u32 *check, u32 *miss, u32 *stall) {
    if (check) *check = gc_gx_vcache_sel;
    if (miss) *miss = 0;
    if (stall) *stall = 0;
}

void GXClearPixMetric(void) {
    for (u32 i = 0; i < 6; i++) gc_gx_pix_metrics[i] = 0;
}

void GXReadPixMetric(u32 *top_in, u32 *top_out, u32 *bot_in, u32 *bot_out, u32 *clr_in, u32 *copy_clks) {
    if (top_in) *top_in = gc_gx_pix_metrics[0];
    if (top_out) *top_out = gc_gx_pix_metrics[1];
    if (bot_in) *bot_in = gc_gx_pix_metrics[2];
    if (bot_out) *bot_out = gc_gx_pix_metrics[3];
    if (clr_in) *clr_in = gc_gx_pix_metrics[4];
    if (copy_clks) *copy_clks = gc_gx_pix_metrics[5];
}

void GXClearMemMetric(void) {
    for (u32 i = 0; i < 10; i++) gc_gx_mem_metrics[i] = 0;
}

void GXReadMemMetric(u32 *cp_req, u32 *tc_req, u32 *cpu_rd_req, u32 *cpu_wr_req,
                     u32 *dsp_req, u32 *io_req, u32 *vi_req, u32 *pe_req, u32 *rf_req, u32 *fi_req) {
    if (cp_req) *cp_req = gc_gx_mem_metrics[0];
    if (tc_req) *tc_req = gc_gx_mem_metrics[1];
    if (cpu_rd_req) *cpu_rd_req = gc_gx_mem_metrics[2];
    if (cpu_wr_req) *cpu_wr_req = gc_gx_mem_metrics[3];
    if (dsp_req) *dsp_req = gc_gx_mem_metrics[4];
    if (io_req) *io_req = gc_gx_mem_metrics[5];
    if (vi_req) *vi_req = gc_gx_mem_metrics[6];
    if (pe_req) *pe_req = gc_gx_mem_metrics[7];
    if (rf_req) *rf_req = gc_gx_mem_metrics[8];
    if (fi_req) *fi_req = gc_gx_mem_metrics[9];
}

// ---- Batch 1 (MP4 callsite-driven) GX APIs ----

// GXAttr constants used by our tests. We only need values for the branches we exercise.
enum {
    GX_VA_PNMTXIDX = 0,
    GX_VA_TEX0MTXIDX = 1,
    GX_VA_TEX1MTXIDX = 2,
    GX_VA_TEX2MTXIDX = 3,
    GX_VA_TEX3MTXIDX = 4,
    GX_VA_TEX4MTXIDX = 5,
    GX_VA_TEX5MTXIDX = 6,
    GX_VA_TEX6MTXIDX = 7,
    GX_VA_TEX7MTXIDX = 8,
    GX_VA_POS = 9,
    GX_VA_NRM = 10,
    GX_VA_CLR0 = 11,
    GX_VA_CLR1 = 12,
    GX_VA_TEX0 = 13,
    GX_VA_TEX1 = 14,
    GX_VA_TEX2 = 15,
    GX_VA_TEX3 = 16,
    GX_VA_TEX4 = 17,
    GX_VA_TEX5 = 18,
    GX_VA_TEX6 = 19,
    GX_VA_TEX7 = 20,
    GX_VA_NBT = 25,
};

static inline void gc_gx_set_vcd_attr(u32 attr, u32 type) {
    switch (attr) {
    case GX_VA_PNMTXIDX:   gc_gx_vcd_lo = set_field(gc_gx_vcd_lo, 1, 0, type); break;
    case GX_VA_TEX0MTXIDX: gc_gx_vcd_lo = set_field(gc_gx_vcd_lo, 1, 1, type); break;
    case GX_VA_TEX1MTXIDX: gc_gx_vcd_lo = set_field(gc_gx_vcd_lo, 1, 2, type); break;
    case GX_VA_TEX2MTXIDX: gc_gx_vcd_lo = set_field(gc_gx_vcd_lo, 1, 3, type); break;
    case GX_VA_TEX3MTXIDX: gc_gx_vcd_lo = set_field(gc_gx_vcd_lo, 1, 4, type); break;
    case GX_VA_TEX4MTXIDX: gc_gx_vcd_lo = set_field(gc_gx_vcd_lo, 1, 5, type); break;
    case GX_VA_TEX5MTXIDX: gc_gx_vcd_lo = set_field(gc_gx_vcd_lo, 1, 6, type); break;
    case GX_VA_TEX6MTXIDX: gc_gx_vcd_lo = set_field(gc_gx_vcd_lo, 1, 7, type); break;
    case GX_VA_TEX7MTXIDX: gc_gx_vcd_lo = set_field(gc_gx_vcd_lo, 1, 8, type); break;
    case GX_VA_POS:        gc_gx_vcd_lo = set_field(gc_gx_vcd_lo, 2, 9, type); break;
    case GX_VA_NRM:
        if (type != 0) { gc_gx_has_nrms = 1; gc_gx_has_binrms = 0; gc_gx_nrm_type = type; }
        else { gc_gx_has_nrms = 0; }
        break;
    case GX_VA_NBT:
        if (type != 0) { gc_gx_has_binrms = 1; gc_gx_has_nrms = 0; gc_gx_nrm_type = type; }
        else { gc_gx_has_binrms = 0; }
        break;
    case GX_VA_CLR0: gc_gx_vcd_lo = set_field(gc_gx_vcd_lo, 2, 13, type); break;
    case GX_VA_CLR1: gc_gx_vcd_lo = set_field(gc_gx_vcd_lo, 2, 15, type); break;
    case GX_VA_TEX0: gc_gx_vcd_hi = set_field(gc_gx_vcd_hi, 2, 0, type); break;
    case GX_VA_TEX1: gc_gx_vcd_hi = set_field(gc_gx_vcd_hi, 2, 2, type); break;
    case GX_VA_TEX2: gc_gx_vcd_hi = set_field(gc_gx_vcd_hi, 2, 4, type); break;
    case GX_VA_TEX3: gc_gx_vcd_hi = set_field(gc_gx_vcd_hi, 2, 6, type); break;
    case GX_VA_TEX4: gc_gx_vcd_hi = set_field(gc_gx_vcd_hi, 2, 8, type); break;
    case GX_VA_TEX5: gc_gx_vcd_hi = set_field(gc_gx_vcd_hi, 2, 10, type); break;
    case GX_VA_TEX6: gc_gx_vcd_hi = set_field(gc_gx_vcd_hi, 2, 12, type); break;
    case GX_VA_TEX7: gc_gx_vcd_hi = set_field(gc_gx_vcd_hi, 2, 14, type); break;
    default: break;
    }
}

void GXSetVtxDesc(u32 attr, u32 type) {
    gc_gx_set_vcd_attr(attr, type);
    if (gc_gx_has_nrms || gc_gx_has_binrms) {
        gc_gx_vcd_lo = set_field(gc_gx_vcd_lo, 2, 11, gc_gx_nrm_type);
    } else {
        gc_gx_vcd_lo = set_field(gc_gx_vcd_lo, 2, 11, 0);
    }
    gc_gx_dirty_state |= 8u;
}

static inline void gc_gx_set_vat(u32 *va, u32 *vb, u32 *vc, u32 attr, u32 cnt, u32 type, u8 shft) {
    switch (attr) {
    case GX_VA_POS:
        *va = set_field(*va, 1, 0, cnt);
        *va = set_field(*va, 3, 1, type);
        *va = set_field(*va, 5, 4, shft);
        break;
    case GX_VA_NRM:
    case GX_VA_NBT:
        *va = set_field(*va, 3, 10, type);
        if (cnt == 2) { // GX_NRM_NBT3
            *va = set_field(*va, 1, 9, 1);
            *va = set_field(*va, 1, 31, 1);
        } else {
            *va = set_field(*va, 1, 9, cnt);
            *va = set_field(*va, 1, 31, 0);
        }
        break;
    case GX_VA_CLR0:
        *va = set_field(*va, 1, 13, cnt);
        *va = set_field(*va, 3, 14, type);
        break;
    case GX_VA_CLR1:
        *va = set_field(*va, 1, 17, cnt);
        *va = set_field(*va, 3, 18, type);
        break;
    case GX_VA_TEX0:
        *va = set_field(*va, 1, 21, cnt);
        *va = set_field(*va, 3, 22, type);
        *va = set_field(*va, 5, 25, shft);
        break;
    case GX_VA_TEX1:
        *vb = set_field(*vb, 1, 0, cnt);
        *vb = set_field(*vb, 3, 1, type);
        *vb = set_field(*vb, 5, 4, shft);
        break;
    case GX_VA_TEX2:
        *vb = set_field(*vb, 1, 9, cnt);
        *vb = set_field(*vb, 3, 10, type);
        *vb = set_field(*vb, 5, 13, shft);
        break;
    case GX_VA_TEX3:
        *vb = set_field(*vb, 1, 18, cnt);
        *vb = set_field(*vb, 3, 19, type);
        *vb = set_field(*vb, 5, 22, shft);
        break;
    case GX_VA_TEX4:
        *vb = set_field(*vb, 1, 27, cnt);
        *vb = set_field(*vb, 3, 28, type);
        *vc = set_field(*vc, 5, 0, shft);
        break;
    case GX_VA_TEX5:
        *vc = set_field(*vc, 1, 5, cnt);
        *vc = set_field(*vc, 3, 6, type);
        *vc = set_field(*vc, 5, 9, shft);
        break;
    case GX_VA_TEX6:
        *vc = set_field(*vc, 1, 14, cnt);
        *vc = set_field(*vc, 3, 15, type);
        *vc = set_field(*vc, 5, 18, shft);
        break;
    case GX_VA_TEX7:
        *vc = set_field(*vc, 1, 23, cnt);
        *vc = set_field(*vc, 3, 24, type);
        *vc = set_field(*vc, 5, 27, shft);
        break;
    default:
        break;
    }
}

void GXSetVtxAttrFmt(u32 vtxfmt, u32 attr, u32 cnt, u32 type, u8 frac) {
    if (vtxfmt >= 8) return;
    u32 *va = &gc_gx_vat_a[vtxfmt];
    u32 *vb = &gc_gx_vat_b[vtxfmt];
    u32 *vc = &gc_gx_vat_c[vtxfmt];
    gc_gx_set_vat(va, vb, vc, attr, cnt, type, frac);
    gc_gx_dirty_state |= 0x10u;
    gc_gx_dirty_vat |= (u32)(1u << (u8)vtxfmt);
}

void GXSetArray(u32 attr, const void *base_ptr, u8 stride) {
    // Mirror SDK: attr==NBT aliases to NRM.
    if (attr == GX_VA_NBT) attr = GX_VA_NRM;
    if (attr < GX_VA_POS) return;
    u32 cp_attr = attr - GX_VA_POS;
    if (cp_attr >= 32) return;
    u32 phy_addr = (u32)(uintptr_t)base_ptr & 0x3FFFFFFFu;
    gc_gx_array_base[cp_attr] = phy_addr;
    gc_gx_array_stride[cp_attr] = (u32)stride;
}

static inline u32 f32_bits(float f) {
    union { float f; u32 u; } u;
    u.f = f;
    return u.u;
}

void GXPosition3f32(float x, float y, float z) {
    gc_gx_pos3f32_x_bits = f32_bits(x);
    gc_gx_pos3f32_y_bits = f32_bits(y);
    gc_gx_pos3f32_z_bits = f32_bits(z);
}

void GXPosition1x16(u16 x) {
    gc_gx_pos1x16_last = (u32)x;
}

void GXPosition2s16(s16 x, s16 y) {
    // Deterministic host model: keep last written values.
    // Store as 32-bit sign-extended values (matches how callers typically
    // promote s16 when doing comparisons/logging).
    gc_gx_pos2s16_x = (u32)(s32)x;
    gc_gx_pos2s16_y = (u32)(s32)y;
}

void GXPosition2u16(u16 x, u16 y) {
    // Deterministic host model: keep last written values (zero-extended).
    gc_gx_pos2u16_x = (u32)x;
    gc_gx_pos2u16_y = (u32)y;
}

void GXPosition3s16(s16 x, s16 y, s16 z) {
    // Deterministic host model: keep last written values (sign-extended).
    gc_gx_pos3s16_x = (u32)(s32)x;
    gc_gx_pos3s16_y = (u32)(s32)y;
    gc_gx_pos3s16_z = (u32)(s32)z;
}

void GXPosition2f32(float x, float y) {
    // Deterministic host model: keep last written values as raw f32 bits.
    gc_gx_pos2f32_x_bits = f32_bits(x);
    gc_gx_pos2f32_y_bits = f32_bits(y);
}

void GXTexCoord2f32(float s, float t) {
    // Deterministic host model: keep last written values as raw f32 bits.
    gc_gx_texcoord2f32_s_bits = f32_bits(s);
    gc_gx_texcoord2f32_t_bits = f32_bits(t);
}

void GXColor1x8(u8 c) {
    // Deterministic host model: record last 8-bit color value.
    gc_gx_color1x8_last = (u32)c;
}

void GXColor3u8(u8 r, u8 g, u8 b) {
    // Deterministic host model: record last RGB triple packed as 0x00RRGGBB.
    gc_gx_color3u8_last = ((u32)r << 16) | ((u32)g << 8) | (u32)b;
}

void GXSetTevColorIn(u32 stage, u32 a, u32 b, u32 c, u32 d) {
    if (stage >= 16) return;
    u32 reg = gc_gx_tevc[stage];
    reg = set_field(reg, 4, 12, a);
    reg = set_field(reg, 4, 8, b);
    reg = set_field(reg, 4, 4, c);
    reg = set_field(reg, 4, 0, d);
    gc_gx_tevc[stage] = reg;
    gx_write_ras_reg(reg);
    gc_gx_bp_sent_not = 0;
}

void GXSetTevAlphaIn(u32 stage, u32 a, u32 b, u32 c, u32 d) {
    if (stage >= 16) return;
    u32 reg = gc_gx_teva[stage];
    reg = set_field(reg, 3, 13, a);
    reg = set_field(reg, 3, 10, b);
    reg = set_field(reg, 3, 7, c);
    reg = set_field(reg, 3, 4, d);
    gc_gx_teva[stage] = reg;
    gx_write_ras_reg(reg);
    gc_gx_bp_sent_not = 0;
}

void GXSetTevColorOp(u32 stage, u32 op, u32 bias, u32 scale, u32 clamp, u32 out_reg) {
    if (stage >= 16) return;
    u32 reg = gc_gx_tevc[stage];
    reg = set_field(reg, 1, 18, op & 1u);
    if (op <= 1) {
        reg = set_field(reg, 2, 20, scale);
        reg = set_field(reg, 2, 16, bias);
    } else {
        reg = set_field(reg, 2, 20, (op >> 1) & 3u);
        reg = set_field(reg, 2, 16, 3u);
    }
    reg = set_field(reg, 1, 19, clamp & 0xFFu);
    reg = set_field(reg, 2, 22, out_reg);
    gc_gx_tevc[stage] = reg;
    gx_write_ras_reg(reg);
    gc_gx_bp_sent_not = 0;
}

void GXSetTevAlphaOp(u32 stage, u32 op, u32 bias, u32 scale, u32 clamp, u32 out_reg) {
    if (stage >= 16) return;
    u32 reg = gc_gx_teva[stage];
    reg = set_field(reg, 1, 18, op & 1u);
    if (op <= 1) {
        reg = set_field(reg, 2, 20, scale);
        reg = set_field(reg, 2, 16, bias);
    } else {
        reg = set_field(reg, 2, 20, (op >> 1) & 3u);
        reg = set_field(reg, 2, 16, 3u);
    }
    reg = set_field(reg, 1, 19, clamp & 0xFFu);
    reg = set_field(reg, 2, 22, out_reg);
    gc_gx_teva[stage] = reg;
    gx_write_ras_reg(reg);
    gc_gx_bp_sent_not = 0;
}

void GXSetTevColor(u32 id, GXColor color) {
    // Mirror decomp_mario_party_4/src/dolphin/gx/GXTev.c:GXSetTevColor observable packed RAS regs.
    u32 regRA = 0;
    u32 regBG = 0;

    regRA = set_field(regRA, 11, 0, (u32)color.r);
    regRA = set_field(regRA, 11, 12, (u32)color.a);
    regRA = set_field(regRA, 8, 24, (u32)(224u + id * 2u));

    regBG = set_field(regBG, 11, 0, (u32)color.b);
    regBG = set_field(regBG, 11, 12, (u32)color.g);
    regBG = set_field(regBG, 8, 24, (u32)(225u + id * 2u));

    gc_gx_tev_color_reg_ra_last = regRA;
    gc_gx_tev_color_reg_bg_last = regBG;

    // SDK writes regRA then regBG three times; last write wins.
    gx_write_ras_reg(regRA);
    gx_write_ras_reg(regBG);
    gx_write_ras_reg(regBG);
    gx_write_ras_reg(regBG);
    gc_gx_bp_sent_not = 0;
}

void GXSetTevOrder(u32 stage, u32 coord, u32 map, u32 color) {
    static const int c2r[] = { 0, 1, 0, 1, 0, 1, 7, 5, 6 };
    if (stage >= 16) return;

    u32 *ptref = &gc_gx_tref[stage / 2];
    gc_gx_texmap_id[stage] = map;

    u32 tmap = map & ~0x100u;
    if (tmap >= 8u) tmap = 0; // GX_TEXMAP0

    u32 tcoord;
    if (coord >= 8u) {
        tcoord = 0; // GX_TEXCOORD0
        gc_gx_tev_tc_enab &= ~(1u << stage);
    } else {
        tcoord = coord;
        gc_gx_tev_tc_enab |= (1u << stage);
    }

    u32 rasc = (color == 0xFFu) ? 7u : (u32)c2r[color];
    u32 enable = (map != 0xFFu) && ((map & 0x100u) == 0);

    if (stage & 1u) {
        *ptref = set_field(*ptref, 3, 12, tmap);
        *ptref = set_field(*ptref, 3, 15, tcoord);
        *ptref = set_field(*ptref, 3, 19, rasc);
        *ptref = set_field(*ptref, 1, 18, enable);
    } else {
        *ptref = set_field(*ptref, 3, 0, tmap);
        *ptref = set_field(*ptref, 3, 3, tcoord);
        *ptref = set_field(*ptref, 3, 7, rasc);
        *ptref = set_field(*ptref, 1, 6, enable);
    }

    gc_gx_bp_sent_not = 0;
    gc_gx_dirty_state |= 1u;
}

/* ═══════════════════════════════════════════════════════════════════
 * GXProject — 3D projection math (GXTransform.c)
 *
 * Source: external/mp4-decomp/src/dolphin/gx/GXTransform.c
 * Pure computation: modelview × projection × viewport transform.
 * ═══════════════════════════════════════════════════════════════════ */

void GXProject(f32 x, f32 y, f32 z,
               const f32 mtx[3][4], const f32 *pm, const f32 *vp,
               f32 *sx, f32 *sy, f32 *sz)
{
    f32 peye_x, peye_y, peye_z;
    f32 xc, yc, zc, wc;

    peye_x = mtx[0][3] + ((mtx[0][2] * z) + ((mtx[0][0] * x) + (mtx[0][1] * y)));
    peye_y = mtx[1][3] + ((mtx[1][2] * z) + ((mtx[1][0] * x) + (mtx[1][1] * y)));
    peye_z = mtx[2][3] + ((mtx[2][2] * z) + ((mtx[2][0] * x) + (mtx[2][1] * y)));

    if (pm[0] == 0.0f) {
        xc = (peye_x * pm[1]) + (peye_z * pm[2]);
        yc = (peye_y * pm[3]) + (peye_z * pm[4]);
        zc = pm[6] + (peye_z * pm[5]);
        wc = 1.0f / -peye_z;
    } else {
        xc = pm[2] + (peye_x * pm[1]);
        yc = pm[4] + (peye_y * pm[3]);
        zc = pm[6] + (peye_z * pm[5]);
        wc = 1.0f;
    }

    *sx = (vp[2] / 2.0f) + (vp[0] + (wc * (xc * vp[2] / 2.0f)));
    *sy = (vp[3] / 2.0f) + (vp[1] + (wc * (-yc * vp[3] / 2.0f)));
    *sz = vp[5] + (wc * (zc * (vp[5] - vp[4])));
}

/* ═══════════════════════════════════════════════════════════════════
 * __cntlzw — portable count-leading-zeros (replaces PPC __cntlzw)
 * ═══════════════════════════════════════════════════════════════════ */

static u32 __cntlzw(u32 x) {
    if (x == 0) return 32;
#if defined(__GNUC__) || defined(__clang__)
    return (u32)__builtin_clz(x);
#else
    u32 n = 0;
    if (x <= 0x0000FFFFu) { n += 16; x <<= 16; }
    if (x <= 0x00FFFFFFu) { n += 8;  x <<= 8;  }
    if (x <= 0x0FFFFFFFu) { n += 4;  x <<= 4;  }
    if (x <= 0x3FFFFFFFu) { n += 2;  x <<= 2;  }
    if (x <= 0x7FFFFFFFu) { n += 1; }
    return n;
#endif
}

/* ═══════════════════════════════════════════════════════════════════
 * GXCompressZ16 / GXDecompressZ16 — Z-depth 24↔16 bit encoding
 *
 * Source: external/mp4-decomp/src/dolphin/gx/GXMisc.c
 * Pure computation (no hardware dependencies).
 * ═══════════════════════════════════════════════════════════════════ */

#define GX_ZC_LINEAR  0
#define GX_ZC_NEAR    1
#define GX_ZC_MID     2
#define GX_ZC_FAR     3

u32 GXCompressZ16(u32 z24, u32 zfmt)
{
    u32 z16;
    u32 z24n;
    s32 exp;
    s32 shift;
    s32 temp;

    z24n = ~(z24 << 8);
    temp = (s32)__cntlzw(z24n);
    switch (zfmt) {
        case GX_ZC_LINEAR:
            z16 = (z24 >> 8) & 0xFFFF;
            break;
        case GX_ZC_NEAR:
            exp = (temp > 3) ? 3 : temp;
            shift = (exp == 3) ? 7 : (9 - exp);
            z16 = ((z24 >> shift) & 0x3FFF) | ((u32)exp << 14);
            break;
        case GX_ZC_MID:
            exp = (temp > 7) ? 7 : temp;
            shift = (exp == 7) ? 4 : (10 - exp);
            z16 = ((z24 >> shift) & 0x1FFF) | ((u32)exp << 13);
            break;
        case GX_ZC_FAR:
            exp = (temp > 12) ? 12 : temp;
            shift = (exp == 12) ? 0 : (11 - exp);
            z16 = ((z24 >> shift) & 0xFFF) | ((u32)exp << 12);
            break;
        default:
            z16 = 0;
            break;
    }
    return z16;
}

u32 GXDecompressZ16(u32 z16, u32 zfmt)
{
    u32 z24;
    u32 cb1;
    s32 exp;
    s32 shift;

    switch (zfmt) {
        case GX_ZC_LINEAR:
            z24 = (z16 << 8) & 0xFFFF00;
            break;
        case GX_ZC_NEAR:
            exp = (s32)((z16 >> 14) & 3);
            shift = (exp == 3) ? 7 : (9 - exp);
            cb1 = (u32)((s32)-1 << (24 - exp));
            z24 = (cb1 | ((z16 & 0x3FFF) << shift)) & 0xFFFFFF;
            break;
        case GX_ZC_MID:
            exp = (s32)((z16 >> 13) & 7);
            shift = (exp == 7) ? 4 : (10 - exp);
            cb1 = (u32)((s32)-1 << (24 - exp));
            z24 = (cb1 | ((z16 & 0x1FFF) << shift)) & 0xFFFFFF;
            break;
        case GX_ZC_FAR:
            exp = (s32)((z16 >> 12) & 0xF);
            shift = (exp == 12) ? 0 : (11 - exp);
            cb1 = (u32)((s32)-1 << (24 - exp));
            z24 = (cb1 | ((z16 & 0xFFF) << shift)) & 0xFFFFFF;
            break;
        default:
            z24 = 0;
            break;
    }
    return z24;
}
