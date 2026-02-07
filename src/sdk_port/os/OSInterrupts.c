#include <stdint.h>

// Minimal interrupt enable/disable state.
//
// We do NOT emulate the PPC MSR here. We only need deterministic behavior for:
// - critical sections (disable -> do work -> restore)
// - nested disable/restore (disable inside already-disabled region)
//
// Evidence: decomp_mario_party_4/src/dolphin/os/OSInterrupt.c

#include "../sdk_state.h"

static uint32_t gc_os_ints_enabled = 1; // default: enabled

int OSDisableInterrupts(void) {
    uint32_t enabled = gc_sdk_state_load_u32_or(GC_SDK_OFF_OS_INTS_ENABLED, gc_os_ints_enabled);
    uint32_t calls = gc_sdk_state_load_u32_or(GC_SDK_OFF_OS_DISABLE_CALLS, 0);
    calls++;
    gc_sdk_state_store_u32_mirror(GC_SDK_OFF_OS_DISABLE_CALLS, 0, calls);

    gc_os_ints_enabled = 0;
    gc_sdk_state_store_u32_mirror(GC_SDK_OFF_OS_INTS_ENABLED, &gc_os_ints_enabled, 0);
    return (enabled != 0);
}

int OSEnableInterrupts(void) {
    uint32_t enabled = gc_sdk_state_load_u32_or(GC_SDK_OFF_OS_INTS_ENABLED, gc_os_ints_enabled);
    gc_os_ints_enabled = 1;
    gc_sdk_state_store_u32_mirror(GC_SDK_OFF_OS_INTS_ENABLED, &gc_os_ints_enabled, 1);
    return (enabled != 0);
}

int OSRestoreInterrupts(int level) {
    uint32_t enabled = gc_sdk_state_load_u32_or(GC_SDK_OFF_OS_INTS_ENABLED, gc_os_ints_enabled);
    uint32_t calls = gc_sdk_state_load_u32_or(GC_SDK_OFF_OS_RESTORE_CALLS, 0);
    calls++;
    gc_sdk_state_store_u32_mirror(GC_SDK_OFF_OS_RESTORE_CALLS, 0, calls);

    gc_os_ints_enabled = (level != 0) ? 1u : 0u;
    gc_sdk_state_store_u32_mirror(GC_SDK_OFF_OS_INTS_ENABLED, &gc_os_ints_enabled, gc_os_ints_enabled);
    return (enabled != 0);
}

