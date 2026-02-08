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

// Constants used by init.c
#define GX_TRUE 1
#define GX_FALSE 0

#define GX_PF_RGB565_Z16 0
#define GX_PF_RGB8_Z24 1
#define GX_ZC_LINEAR 0

#define GX_GM_1_0 0

#define GX_LEQUAL 3

