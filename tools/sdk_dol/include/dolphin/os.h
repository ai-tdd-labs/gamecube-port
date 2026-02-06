#ifndef _DOLPHIN_OS
#define _DOLPHIN_OS

#include "dolphin/types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OS_BASE_CACHED 0x80000000
#define OSPhysicalToCached(paddr) ((void *)((u32)(paddr) + OS_BASE_CACHED))

typedef s64 OSTime;
typedef u32 OSTick;

typedef struct OSThreadQueue {
    struct OSThreadQueue *head;
    struct OSThreadQueue *tail;
} OSThreadQueue;

typedef int __OSInterrupt;

void OSInit(void);
BOOL OSDisableInterrupts(void);
void OSRestoreInterrupts(BOOL level);
void __OSSetInterruptHandler(int interrupt, void *handler);
void __OSUnmaskInterrupts(u32 mask);
void OSSleepThread(OSThreadQueue *queue);
void OSWakeupThread(OSThreadQueue *queue);
void OSInitThreadQueue(OSThreadQueue *queue);
void OSReport(const char *fmt, ...);
void OSPanic(const char *file, int line, const char *fmt, ...);

#define OSRoundUp32B(x) (((u32)(x) + 0x1F) & ~(0x1F))
#define OSMillisecondsToTicks(msec) ((msec) * 1)

#ifdef __cplusplus
}
#endif

#endif // _DOLPHIN_OS
