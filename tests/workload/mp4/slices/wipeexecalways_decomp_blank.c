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

// We intentionally do not emulate GX drawing on host workloads yet.
static void WipeColorFill(GXColor color) { (void)color; }
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
