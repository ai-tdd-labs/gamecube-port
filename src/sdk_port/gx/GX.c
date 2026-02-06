#include <stdint.h>

typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;

// Minimal GX state mirror. We only model fields asserted by our deterministic tests.
u32 gc_gx_in_disp_list;
u32 gc_gx_dl_save_context;
u32 gc_gx_gen_mode;
u32 gc_gx_bp_mask;

u32 gc_gx_bp_sent_not;
float gc_gx_vp_left;
float gc_gx_vp_top;
float gc_gx_vp_wd;
float gc_gx_vp_ht;
float gc_gx_vp_nearz;
float gc_gx_vp_farz;

u32 gc_gx_su_scis0;
u32 gc_gx_su_scis1;

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

typedef struct {
    uint8_t _dummy;
} GXFifoObj;

static GXFifoObj s_fifo_obj;

// Helpers to match SDK bitfield packing macros.
static inline u32 set_field(u32 reg, u32 size, u32 shift, u32 v) {
    const u32 mask = ((1u << size) - 1u) << shift;
    return (reg & ~mask) | ((v << shift) & mask);
}

GXFifoObj *GXInit(void *base, u32 size) {
    (void)base;
    (void)size;
    gc_gx_in_disp_list = 0;
    gc_gx_dl_save_context = 1;
    gc_gx_gen_mode = 0;
    gc_gx_bp_mask = 0xFF;

    // Keep deterministic defaults for higher-level GX state tracked by our tests.
    gc_gx_vcd_lo = 0;
    gc_gx_vcd_hi = 0;
    gc_gx_has_nrms = 0;
    gc_gx_has_binrms = 0;
    gc_gx_nrm_type = 0;
    gc_gx_dirty_state = 0;
    gc_gx_dirty_vat = 0;
    for (u32 i = 0; i < 8; i++) {
        gc_gx_vat_a[i] = 0;
        gc_gx_vat_b[i] = 0;
        gc_gx_vat_c[i] = 0;
    }
    for (u32 i = 0; i < 32; i++) {
        gc_gx_array_base[i] = 0;
        gc_gx_array_stride[i] = 0;
    }
    for (u32 i = 0; i < 16; i++) {
        gc_gx_tevc[i] = 0;
        gc_gx_teva[i] = 0;
        gc_gx_texmap_id[i] = 0;
    }
    for (u32 i = 0; i < 8; i++) {
        gc_gx_tref[i] = 0;
    }
    gc_gx_tev_tc_enab = 0;
    gc_gx_pos3f32_x_bits = 0;
    gc_gx_pos3f32_y_bits = 0;
    gc_gx_pos3f32_z_bits = 0;
    gc_gx_pos1x16_last = 0;

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

void GXSetDispCopySrc(u16 left, u16 top, u16 wd, u16 ht) {
    u32 reg = 0;
    reg = set_field(reg, 10, 0, left);
    reg = set_field(reg, 10, 10, top);
    reg = set_field(reg, 8, 24, 0x49);
    gc_gx_cp_disp_src = reg;

    reg = 0;
    reg = set_field(reg, 10, 0, (u32)(wd - 1u));
    reg = set_field(reg, 10, 10, (u32)(ht - 1u));
    reg = set_field(reg, 8, 24, 0x4A);
    gc_gx_cp_disp_size = reg;

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
    gc_gx_copy_disp_dest = (u32)(uintptr_t)dest;
    gc_gx_copy_disp_clear = (u32)clear;
}

void GXSetDispCopyGamma(u32 gamma) {
    gc_gx_copy_gamma = gamma;
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

void GXSetZMode(u8 enable, u32 func, u8 update_enable) {
    gc_gx_zmode_enable = (u32)enable;
    gc_gx_zmode_func = func;
    gc_gx_zmode_update_enable = (u32)update_enable;
}

void GXSetColorUpdate(u8 enable) {
    gc_gx_color_update_enable = (u32)enable;
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
