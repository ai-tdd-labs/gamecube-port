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
