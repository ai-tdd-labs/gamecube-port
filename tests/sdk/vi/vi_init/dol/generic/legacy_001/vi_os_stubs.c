#include <dolphin/types.h>
#include <dolphin/os/OSContext.h>
#include <dolphin/os/OSThread.h>
#include <dolphin/OSRtcPriv.h>

typedef s16 __OSInterrupt;
typedef void (*__OSInterruptHandler)(__OSInterrupt interrupt, OSContext *context);
typedef u32 OSInterruptMask;

void OSInit(void) {}
void OSReport(const char *fmt, ...) { (void)fmt; }
BOOL OSDisableInterrupts(void) { return TRUE; }
void OSRestoreInterrupts(BOOL level) { (void)level; }
void OSSetCurrentContext(OSContext *context) { (void)context; }
void OSClearContext(OSContext *context) { (void)context; }
void OSInitThreadQueue(OSThreadQueue *queue) { (void)queue; }
void OSSleepThread(OSThreadQueue *queue) { (void)queue; }
void OSWakeupThread(OSThreadQueue *queue) { (void)queue; }

static OSSram g_sram;
OSSram *__OSLockSram(void) { return &g_sram; }
BOOL __OSUnlockSram(BOOL commit) { (void)commit; return TRUE; }

__OSInterruptHandler __OSSetInterruptHandler(__OSInterrupt interrupt, __OSInterruptHandler handler) {
    (void)interrupt;
    return handler;
}
OSInterruptMask __OSUnmaskInterrupts(OSInterruptMask mask) { return mask; }
