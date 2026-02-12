#include <stdint.h>

#include "src/sdk_port/sdk_state.h"

typedef int32_t s32;
typedef uint32_t u32;

static u32 s_pad_motor_cmd[4];

void PADControlMotor(s32 chan, u32 command) {
    if (chan < 0 || chan >= 4) {
        return;
    }
    s_pad_motor_cmd[(u32)chan] = command;
    gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_MOTOR_CMD_BASE + (u32)chan * 4u, command);
}
