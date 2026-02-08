#pragma once

#include "dolphin/types.h"

// Minimal GX header surface for MP4 workload compilation (HuSysInit path).

typedef struct GXRenderModeObj {
    u32 viTVmode;
    u16 fbWidth;
    u16 efbHeight;
    u16 xfbHeight;
    u8 aa;
    u8 field_rendering;
    u8 sample_pattern[12 * 2]; // opaque, just sized to avoid NULL access in InitGX
    u8 vfilter[7];
} GXRenderModeObj;

typedef struct GXFifoObj {
    u8 _dummy;
} GXFifoObj;

typedef struct GXColor {
    u8 r, g, b, a;
} GXColor;

extern GXRenderModeObj GXNtsc480IntDf;
extern GXRenderModeObj GXNtsc480Prog;
extern GXRenderModeObj GXPal528IntDf;
extern GXRenderModeObj GXMpal480IntDf;

// GX init / video setup.
GXFifoObj *GXInit(void *base, u32 size);
void GXAdjustForOverscan(GXRenderModeObj *src, GXRenderModeObj *dst, u16 hor, u16 ver);
void GXSetViewport(f32 left, f32 top, f32 wd, f32 ht, f32 nearz, f32 farz);
void GXSetViewportJitter(f32 left, f32 top, f32 wd, f32 ht, f32 nearz, f32 farz, u32 field);
void GXSetScissor(u32 left, u32 top, u32 wd, u32 ht);
void GXSetProjection(f32 mtx[4][4], u32 type);
void GXLoadPosMtxImm(f32 mtx[3][4], u32 id);
void GXSetCurrentMtx(u32 id);
void GXSetDispCopySrc(u32 left, u32 top, u32 wd, u32 ht);
void GXSetDispCopyDst(u32 wd, u32 ht);
f32 GXSetDispCopyYScale(f32 yscale);
f32 GXGetYScaleFactor(u16 efbHeight, u16 xfbHeight);
void GXSetCopyFilter(u8 aa, const u8 *sample_pattern, u8 vfilter_enable, const u8 *vfilter);
void GXSetPixelFmt(u32 pixelFmt, u32 zFmt);
void GXCopyDisp(void *dst, u8 clear);
void GXSetDispCopyGamma(u32 gamma);
void GXInvalidateVtxCache(void);
void GXInvalidateTexAll(void);
void GXDrawDone(void);
void GXSetZMode(u8 enable, u32 func, u8 update_enable);
void GXSetColorUpdate(u8 enable);
void GXSetAlphaUpdate(u8 enable);
void GXSetNumChans(u8 nChans);
void GXSetChanMatColor(u32 chan, GXColor mat_color);
void GXSetChanCtrl(u32 chan, u8 enable, u32 amb_src, u32 mat_src, u32 light_mask, u32 diff_fn, u32 attn_fn);
void GXSetTevOrder(u32 stage, u32 coord, u32 map, u32 color);
void GXSetTevOp(u32 stage, u32 mode);
void GXSetNumTexGens(u8 nTexGens);
void GXSetNumTevStages(u8 nStages);
void GXClearVtxDesc(void);
void GXSetVtxDesc(u32 attr, u32 type);
void GXSetVtxAttrFmt(u32 vtxfmt, u32 attr, u32 cnt, u32 type, u8 frac);
void GXBegin(u8 type, u8 vtxfmt, u16 nverts);
void GXEnd(void);
void GXPosition2s16(s16 x, s16 y);

// Constants used by init.c
#define GX_TRUE 1
#define GX_FALSE 0

#define GX_PF_RGB565_Z16 0
#define GX_PF_RGB8_Z24 1
#define GX_ZC_LINEAR 0

#define GX_GM_1_0 0

#define GX_LEQUAL 3
