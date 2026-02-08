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
    case WIPE_MODE_OUT:
        // Not reached in our current host workloads; keep behavior minimal.
        // Full parity requires implementing WipeNormalFade/WipeCrossFade helpers + GX.
        break;
    default:
        break;
    }
}

