#pragma once

// Minimal host-safe init header for compiling src/game_workload/mp4/vendor/src/game/init.c.

struct GXRenderModeObj;
typedef struct GXRenderModeObj GXRenderModeObj;

void HuSysInit(GXRenderModeObj *mode);

// Game-specific hooks called by HuSysInit. Implemented as stubs in the host workload
// scenario; the goal here is SDK reachability, not game correctness.
void HuDvdErrDispInit(GXRenderModeObj *mode, void *fb1, void *fb2);
void HuMemInitAll(void);
void HuAudInit(void);
void HuARInit(void);
void HuCardInit(void);
unsigned int frand(void);

void DEMOUpdateStats(int which);
void DEMOPrintStats(void);
