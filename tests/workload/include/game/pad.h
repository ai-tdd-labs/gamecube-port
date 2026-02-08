#pragma once

// MP4 workload: minimal public pad API used by decomp init/pad code.
#include "dolphin.h"

void HuPadInit(void);
void HuPadRead(void);

// Game-level button aliases used by MP4 pad.c (not part of the Nintendo SDK).
#define PAD_BUTTON_TRIGGER_L 0x4000
#define PAD_BUTTON_TRIGGER_R 0x2000
