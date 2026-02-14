#include <stdint.h>

#include "dolphin/os.h"

// Minimal, deterministic host implementation of OS thread queues.
//
// This is not a real scheduler. It exists so blocking SDK functions like
// __CARDSync can be ported without pulling in the full OSThread model.

uint32_t gc_os_sleep_calls;
uint32_t gc_os_wakeup_calls;

// Optional hook invoked on each OSSleepThread call. Used by deterministic tests
// to advance state (e.g., flip a result from BUSY->READY).
void (*gc_os_sleep_hook)(OSThreadQueue *queue);

void OSSleepThread(OSThreadQueue *queue)
{
    gc_os_sleep_calls++;
    if (gc_os_sleep_hook) {
        gc_os_sleep_hook(queue);
    }
}

void OSWakeupThread(OSThreadQueue *queue)
{
    (void)queue;
    gc_os_wakeup_calls++;
}

