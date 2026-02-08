#include <stdint.h>

typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;

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

u32 gc_gx_cp_disp_src;
u32 gc_gx_cp_disp_size;
u32 gc_gx_cp_disp_stride;
u32 gc_gx_cp_disp;

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

// Immediate-mode vertex helpers (in the real SDK these write to the FIFO).
u32 gc_gx_pos3f32_x_bits;
u32 gc_gx_pos3f32_y_bits;
u32 gc_gx_pos3f32_z_bits;
u32 gc_gx_pos1x16_last;

// Token / draw sync state (GXManage).
uintptr_t gc_gx_token_cb_ptr;
u32 gc_gx_last_draw_sync_token;

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

static inline void gx_write_xf_reg(u32 idx, u32 v) {
    if (idx < (sizeof(gc_gx_xf_regs) / sizeof(gc_gx_xf_regs[0]))) {
        gc_gx_xf_regs[idx] = v;
    }
}

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
    gc_gx_pos3f32_x_bits = 0;
    gc_gx_pos3f32_y_bits = 0;
    gc_gx_pos3f32_z_bits = 0;
    gc_gx_pos1x16_last = 0;
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

typedef struct {
    u8 r, g, b, a;
} GXColor;

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

void GXSetZMode(u8 enable, u32 func, u8 update_enable) {
    gc_gx_zmode_enable = (u32)enable;
    gc_gx_zmode_func = func;
    gc_gx_zmode_update_enable = (u32)update_enable;
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
