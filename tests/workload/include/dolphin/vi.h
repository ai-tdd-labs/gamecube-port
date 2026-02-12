#pragma once

#include "dolphin/types.h"

struct GXRenderModeObj;

typedef void (*VIRetraceCallback)(u32 retraceCount);

void VIInit(void);
void VIConfigure(const struct GXRenderModeObj *rm);
void VIConfigurePan(u16 x, u16 y, u16 w, u16 h);
u32 VIGetTvFormat(void);
u32 VIGetDTVStatus(void);
void VIWaitForRetrace(void);
u32 VIGetRetraceCount(void);
u32 VIGetNextField(void);
void VISetNextFrameBuffer(void *fb);
void VIFlush(void);
VIRetraceCallback VISetPostRetraceCallback(VIRetraceCallback cb);
VIRetraceCallback VISetPreRetraceCallback(VIRetraceCallback cb);
void VISetBlack(u32 black);
