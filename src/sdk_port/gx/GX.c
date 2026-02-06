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
