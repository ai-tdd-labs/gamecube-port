#include <stdint.h>

// Minimal decomp slice for MP4 wipe state machine.
//
// Purpose (host workload only):
// - Replace the inline WipeExecAlways() stub so we execute the real control flow.
// - Keep all GX drawing as NO-OP on host (we are not emulating the GX FIFO here).
//
// Source of truth: decomp_mario_party_4/src/game/wipe.c and include/game/wipe.h

#include "dolphin/gx.h"
#include "dolphin/types.h"

#ifdef GC_HOST_WORKLOAD
#include <stdint.h>
#endif

#define WIPE_TYPE_PREV -1
#define WIPE_TYPE_NORMAL 0
#define WIPE_TYPE_CROSS 1
#define WIPE_TYPE_DUMMY 2
#define WIPE_MODE_IN 1
#define WIPE_MODE_OUT 2
#define WIPE_MODE_BLANK 3

typedef struct wipe_state {
    u32 unk00;
    u32 unk04;
    void *copy_data;
    u32 unk0C;
    void *unk10[8];
    float time;
    float duration;
    u32 unk38;
    u16 w;
    u16 h;
    u16 x;
    u16 y;
    GXColor color;
    volatile u8 type;
    u8 mode;
    u8 stat;
    u8 keep_copy;
} WipeState;

WipeState wipeData;
int wipeFadeInF;

// WipeColorFill/WipeFrameStill are GX+MTX heavy. For host workloads we keep
// rendering as "state setup only". When MTX linking is enabled at build time
// (GC_HOST_WORKLOAD_MTX=1), we run a host-safe subset of the decomp sequence
// to surface deeper GX calls.
#ifdef GC_HOST_WORKLOAD_MTX
#include "dolphin/mtx.h"
#endif

// Extra GX entry points not present in the workload's minimal gx.h.
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;
typedef float f32;

void GXSetProjection(f32 mtx[4][4], u32 type);
void GXLoadPosMtxImm(f32 mtx[3][4], u32 id);
void GXSetCurrentMtx(u32 id);
void GXSetZMode(u8 enable, u32 func, u8 update_enable);
void GXSetAlphaUpdate(u8 enable);
void GXSetColorUpdate(u8 enable);
void GXSetAlphaCompare(u32 comp0, u8 ref0, u32 op, u32 comp1, u8 ref1);
void GXSetBlendMode(u32 type, u32 src_fact, u32 dst_fact, u32 op);
void GXSetChanMatColor(u32 chan, GXColor mat_color);
void GXSetNumTexGens(u8 nTexGens);
void GXEnd(void);
void GXPosition2u16(u16 x, u16 y);

// Numeric constants copied from external/mp4-decomp/include/dolphin/gx/GXEnum.h.
enum {
    GX_ORTHOGRAPHIC = 1,
    GX_PNMTX0 = 0,
    GX_COLOR0A0 = 4,

    GX_SRC_REG = 0,
    GX_DF_NONE = 0,
    GX_AF_NONE = 2,

    GX_TEVSTAGE0 = 0,
    GX_TEXCOORD_NULL = 0xFF,
    GX_TEXMAP_NULL = 0xFF,
    GX_PASSCLR = 4,

    GX_QUADS = 0x80,
    GX_VTXFMT0 = 0,
    GX_VA_POS = 9,
    GX_DIRECT = 1,
    GX_POS_XY = 0,
    GX_U16 = 2,

    GX_GEQUAL = 6,
    GX_ALWAYS = 7,
    GX_AOP_AND = 0,
    GX_BM_BLEND = 1,
    GX_BL_SRCALPHA = 4,
    GX_BL_INVSRCALPHA = 5,
    GX_LO_NOOP = 5,
};

// We intentionally keep the draw calls, but rely on sdk_port's deterministic GX model.
static void WipeColorFill(GXColor color) {
#ifndef GC_HOST_WORKLOAD_MTX
    (void)color;
    return;
#else

    static GXColor colorN = { 0xFF, 0xFF, 0xFF, 0xFF };
    Mtx44 proj;
    Mtx modelview;
    WipeState *wipe = &wipeData;
    u16 ulx = (u16)(s32)wipe->x;
    u16 lrx = (u16)(wipe->x + wipe->w);
    u16 uly = (u16)(s32)wipe->y;
    u16 lry = (u16)(wipe->x + wipe->h + 1);

    MTXOrtho(proj, (f32)uly, (f32)lry, (f32)ulx, (f32)lrx, 0.0f, 10.0f);
    GXSetProjection(proj, (u32)GX_ORTHOGRAPHIC);
    MTXIdentity(modelview);
    GXLoadPosMtxImm(modelview, (u32)GX_PNMTX0);
    GXSetCurrentMtx((u32)GX_PNMTX0);
    GXSetViewport(0, 0, (f32)wipe->w, (f32)(wipe->h + 1), 0, 1);
    GXSetScissor(0, 0, wipe->w, (u32)(wipe->h + 1));
    GXClearVtxDesc();
    GXSetChanMatColor((u32)GX_COLOR0A0, color);
    GXSetNumChans(1);
    GXSetChanCtrl((u32)GX_COLOR0A0, (u8)GX_FALSE, (u32)GX_SRC_REG, (u32)GX_SRC_REG, 0, (u32)GX_DF_NONE, (u32)GX_AF_NONE);
    GXSetTevOrder((u32)GX_TEVSTAGE0, (u32)GX_TEXCOORD_NULL, (u32)GX_TEXMAP_NULL, (u32)GX_COLOR0A0);
    GXSetTevOp((u32)GX_TEVSTAGE0, (u32)GX_PASSCLR);
    GXSetNumTexGens(0);
    GXSetNumTevStages(1);
    GXSetVtxDesc((u32)GX_VA_POS, (u32)GX_DIRECT);
    GXSetVtxAttrFmt((u32)GX_VTXFMT0, (u32)GX_VA_POS, (u32)GX_POS_XY, (u32)GX_U16, 0);
    GXSetZMode((u8)GX_FALSE, (u32)GX_LEQUAL, (u8)GX_FALSE);
    GXSetAlphaUpdate((u8)GX_FALSE);
    GXSetColorUpdate((u8)GX_TRUE);
    GXSetAlphaCompare((u32)GX_GEQUAL, 1, (u32)GX_AOP_AND, (u32)GX_GEQUAL, 1);
    GXSetBlendMode((u32)GX_BM_BLEND, (u32)GX_BL_SRCALPHA, (u32)GX_BL_INVSRCALPHA, (u32)GX_LO_NOOP);
    GXBegin((u8)GX_QUADS, (u8)GX_VTXFMT0, 4);
    GXPosition2u16(ulx, uly);
    GXPosition2u16(lrx, uly);
    GXPosition2u16(lrx, lry);
    GXPosition2u16(ulx, lry);
    GXEnd();
    GXSetChanMatColor((u32)GX_COLOR0A0, colorN);
#endif
}

// Not enabled yet (needs texture copy plumbing): keep it as a no-op even with MTX.
static void WipeFrameStill(GXColor color) { (void)color; }

// Host-only: keep time progression deterministic without needing the full
// HuSysVWaitGet implementation linked into the workload.
#ifdef GC_HOST_WORKLOAD
__attribute__((weak)) s16 HuSysVWaitGet(s16 old) { (void)old; return 1; }
__attribute__((weak)) void HuMemDirectFree(void *ptr) { (void)ptr; }
#endif

typedef s32 (*fadeFunc)(void);

static s32 WipeNormalFade(void);
static s32 WipeCrossFade(void);
static s32 WipeDummyFade(void);

static fadeFunc fadeInFunc[3] = { WipeNormalFade, WipeCrossFade, WipeDummyFade };
static fadeFunc fadeOutFunc[3] = { WipeNormalFade, WipeCrossFade, WipeDummyFade };

static s32 WipeDummyFade(void) { return 0; }

static s32 WipeNormalFade(void)
{
    u8 alpha;
    WipeState *wipe = &wipeData;
    if (wipe->duration == 0) return 0;

    alpha = (u8)((wipe->time / wipe->duration) * 255.0f);
    switch (wipe->mode) {
    case WIPE_MODE_IN:
        wipe->color.a = (u8)(255u - alpha);
        break;
    case WIPE_MODE_OUT:
        wipe->color.a = alpha;
        break;
    default:
        return 0;
    }
    WipeColorFill(wipe->color);
    return (wipe->time >= wipe->duration) ? 0 : 1;
}

// Host workloads: do not model crossfade texture copy yet. Keep the control flow
// deterministic and return "done" immediately (so WipeExecAlways cleanup paths run).
static s32 WipeCrossFade(void) { return 0; }

void WipeInit(GXRenderModeObj *rmode)
{
    WipeState *wipe = &wipeData;
    wipe->unk00 = 0;
    wipe->unk04 = 0;
    wipe->copy_data = 0;
    wipe->stat = WIPE_TYPE_NORMAL;
    wipe->mode = WIPE_MODE_BLANK;
    wipe->type = 0;
    wipe->keep_copy = 0;
    for (int i = 0; i < 8; i++) {
        wipe->unk10[i] = 0;
    }
    wipe->color.r = 0;
    wipe->color.g = 0;
    wipe->color.b = 0;
    wipe->color.a = 0;
    wipe->w = rmode ? rmode->fbWidth : 0;
    wipe->h = rmode ? rmode->efbHeight : 0;
    wipe->x = 0;
    wipe->y = 0;
}

void WipeExecAlways(void)
{
    s32 i;
    WipeState *wipe = &wipeData;
    switch (wipe->mode) {
    case WIPE_MODE_BLANK:
        wipe->type; // decomp artifact / intentional no-op
        wipe->color.a = 255;
        if (wipe->copy_data) {
            WipeFrameStill(wipe->color);
        } else {
            WipeColorFill(wipe->color);
        }
        break;
    case WIPE_MODE_IN:
        if (wipe->type < WIPE_TYPE_DUMMY) {
            wipe->stat = (u8)fadeInFunc[wipe->type]();
        } else {
            wipe->stat = 0;
        }
        wipe->time += (float)HuSysVWaitGet(0);
        if (wipe->stat) return;

        if (wipe->copy_data) {
            if (!wipe->keep_copy) {
                HuMemDirectFree(wipe->copy_data);
            }
            wipe->copy_data = 0;
        }
        for (i = 0; i < 8; i++) {
            if (wipe->unk10[i]) {
                HuMemDirectFree(wipe->unk10[i]);
                wipe->unk10[i] = 0;
            }
        }
        wipe->unk0C = 0;
        wipe->time = 0;
        wipe->mode = 0;
        break;
    case WIPE_MODE_OUT:
        if (wipe->type < WIPE_TYPE_DUMMY) {
            wipe->stat = (u8)fadeOutFunc[wipe->type]();
        } else {
            wipe->stat = 0;
        }
        wipe->time += (float)HuSysVWaitGet(0);
        if (wipe->stat) return;
        wipe->time = 0;
        wipe->mode = WIPE_MODE_BLANK;
        break;
    default:
        break;
    }
}
