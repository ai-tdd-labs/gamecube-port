#include <stdint.h>

#include "src/sdk_port/sdk_state.h"

typedef uint16_t u16;
typedef uint32_t u32;
typedef int8_t s8;
typedef uint8_t u8;

#define PAD_CHAN0_BIT 0x80000000u

enum {
    PAD_ERR_NONE = 0,
    PAD_ERR_NO_CONTROLLER = -1,
};

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
    u8 _pad;
} PADStatus;

u32 PADRead(PADStatus *status) {
    if (status) {
        for (int i = 0; i < 4; i++) {
            status[i].button = 0;
            status[i].stickX = 0;
            status[i].stickY = 0;
            status[i].substickX = 0;
            status[i].substickY = 0;
            status[i].triggerL = 0;
            status[i].triggerR = 0;
            status[i].analogA = 0;
            status[i].analogB = 0;
            status[i].err = (s8)((i == 0) ? PAD_ERR_NONE : PAD_ERR_NO_CONTROLLER);
        }
    }
    gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_READ_CALLS,
                             gc_sdk_state_load_u32_or(GC_SDK_OFF_PAD_READ_CALLS, 0) + 1u);
    return PAD_CHAN0_BIT;
}
