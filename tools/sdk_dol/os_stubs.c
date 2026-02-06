#include <dolphin/types.h>
#include <dolphin/os/OSThread.h>
#include <stdarg.h>

// Minimal stubs for SDK linking

void OSInit(void) {}

void OSReport(const char *fmt, ...) {
    (void)fmt;
}

void OSPanic(const char *file, int line, const char *fmt, ...) {
    (void)file; (void)line; (void)fmt;
    // Halt
    while (1) {}
}

BOOL OSDisableInterrupts(void) { return TRUE; }
void OSRestoreInterrupts(BOOL level) { (void)level; }

void OSSleepThread(OSThreadQueue *queue) { (void)queue; }
void OSWakeupThread(OSThreadQueue *queue) { (void)queue; }
