#pragma once

#include "dolphin/types.h"
#include <stddef.h>
#include <stdint.h>

// Minimal host-safe OS header surface for MP4 workload compilation.

typedef int OSHeapHandle;

void OSInit(void);
void *OSGetArenaLo(void);
void *OSGetArenaHi(void);
void OSSetArenaLo(void *addr);
void OSSetArenaHi(void *addr);
void *OSInitAlloc(void *arena_lo, void *arena_hi, int maxHeaps);
u32 OSCreateHeap(void *start, void *end);
void OSSetCurrentHeap(u32 heap);
void *OSAlloc(u32 size);
void OSReport(const char *fmt, ...);
void OSPanic(const char *file, int line, const char *fmt, ...);

u32 OSGetConsoleType(void);
u32 OSGetPhysicalMemSize(void);
u32 OSGetConsoleSimulatedMemSize(void);

u32 OSGetProgressiveMode(void);

int OSDisableInterrupts(void);
int OSRestoreInterrupts(int level);

void OSInitFastCast(void);

u32 OSRoundUp32B(u32 x);
u32 OSRoundDown32B(u32 x);

// Real SDK signature (see decomp OSAlloc.c): used only in dev-hardware meminfo path.
void *OSAllocFixed(void **rstart, void **rend);
void OSDumpHeap(int heap);

// Real SDK global (implemented in src/sdk_port/os/OSAlloc.c).
extern volatile int32_t __OSCurrHeap;

// Constants used by init.c.
#define OS_CONSOLE_DEVHW1 0x10000000u
