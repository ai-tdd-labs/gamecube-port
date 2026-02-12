#include <stdint.h>

#include "src/sdk_port/sdk_state.h"

typedef uint16_t u16;
typedef uint32_t u32;
typedef int8_t s8;
typedef uint8_t u8;

typedef struct PADStatus {
    u16 button;
    s8 stickX;
    s8 stickY;
    s8 substickX;
    s8 substickY;
    u8 triggerL;
    u8 triggerR;
    u8 analogA;
    u8 analogB;
    s8 err;
} PADStatus;

enum {
    PAD_ERR_NONE = 0,
};

typedef struct PADClampRegion {
    u8 minTrigger;
    u8 maxTrigger;
    s8 minStick;
    s8 maxStick;
    s8 xyStick;
    s8 minSubstick;
    s8 maxSubstick;
    s8 xySubstick;
} PADClampRegion;

static PADClampRegion ClampRegion = {
    30, 180,
    15, 72, 40,
    15, 59, 31,
};

static void ClampStick(s8 *px, s8 *py, s8 max, s8 xy, s8 min) {
    int x = *px;
    int y = *py;
    int signX, signY, d;

    if (0 <= x) { signX = 1; } else { signX = -1; x = -x; }
    if (0 <= y) { signY = 1; } else { signY = -1; y = -y; }
    if (x <= min) { x = 0; } else { x -= min; }
    if (y <= min) { y = 0; } else { y -= min; }

    if (x == 0 && y == 0) { *px = *py = 0; return; }

    if (xy * y <= xy * x) {
        d = xy * x + (max - xy) * y;
        if (xy * max < d) { x = (s8)(xy * max * x / d); y = (s8)(xy * max * y / d); }
    } else {
        d = xy * y + (max - xy) * x;
        if (xy * max < d) { x = (s8)(xy * max * x / d); y = (s8)(xy * max * y / d); }
    }
    *px = (s8)(signX * x);
    *py = (s8)(signY * y);
}

void PADClamp(PADStatus *status) {
    int i;
    for (i = 0; i < 4; i++, status++) {
        if (status->err != PAD_ERR_NONE) continue;
        ClampStick(&status->stickX, &status->stickY,
                   ClampRegion.maxStick, ClampRegion.xyStick, ClampRegion.minStick);
        ClampStick(&status->substickX, &status->substickY,
                   ClampRegion.maxSubstick, ClampRegion.xySubstick, ClampRegion.minSubstick);
        if (status->triggerL <= ClampRegion.minTrigger) {
            status->triggerL = 0;
        } else {
            if (ClampRegion.maxTrigger < status->triggerL)
                status->triggerL = ClampRegion.maxTrigger;
            status->triggerL -= ClampRegion.minTrigger;
        }
        if (status->triggerR <= ClampRegion.minTrigger) {
            status->triggerR = 0;
        } else {
            if (ClampRegion.maxTrigger < status->triggerR)
                status->triggerR = ClampRegion.maxTrigger;
            status->triggerR -= ClampRegion.minTrigger;
        }
    }
    gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_CLAMP_CALLS,
                             gc_sdk_state_load_u32_or(GC_SDK_OFF_PAD_CLAMP_CALLS, 0) + 1u);
}
