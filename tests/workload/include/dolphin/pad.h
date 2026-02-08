#pragma once

#include "dolphin/types.h"

// Minimal PAD header surface for MP4 workload compilation.
void PADSetSpec(u32 spec);
u32 PADInit(void); // some code treats this as returning BOOL; sdk_port ignores return
void PADControlMotor(s32 chan, u32 command);

#define PAD_SPEC_5 5
#define PAD_MOTOR_STOP 0
#define PAD_MOTOR_RUMBLE 1
#define PAD_MOTOR_STOP_HARD 2
