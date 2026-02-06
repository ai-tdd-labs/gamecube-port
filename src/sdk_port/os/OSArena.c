#include <stdint.h>

// Minimal SDK port for Arena pointers.
// Kept intentionally small: tests drive behavior, no refactors.

#include "../sdk_state.h"

// Fallback storage when gc_mem isn't mapped (should be rare in our harnesses).
static void *g_os_arena_lo_fallback = (void *)(uintptr_t)-1;
static void *g_os_arena_hi_fallback = 0;

void *OSGetArenaLo(void) {
    // Prefer RAM-backed state.
    uint32_t v = gc_sdk_state_load_u32be(GC_SDK_OFF_OS_ARENA_LO);
    if (v != 0) return (void *)(uintptr_t)v;
    return g_os_arena_lo_fallback;
}

void *OSGetArenaHi(void) {
    uint32_t v = gc_sdk_state_load_u32be(GC_SDK_OFF_OS_ARENA_HI);
    if (v != 0) return (void *)(uintptr_t)v;
    return g_os_arena_hi_fallback;
}

void OSSetArenaLo(void *addr) {
    uint32_t v = (uint32_t)(uintptr_t)addr;
    // If state page isn't mapped, keep a fallback so host code doesn't break.
    if (!gc_sdk_state_ptr(GC_SDK_OFF_OS_ARENA_LO, 4)) {
        g_os_arena_lo_fallback = addr;
        return;
    }
    gc_sdk_state_store_u32be(GC_SDK_OFF_OS_ARENA_LO, v);
}

void OSSetArenaHi(void *addr) {
    uint32_t v = (uint32_t)(uintptr_t)addr;
    if (!gc_sdk_state_ptr(GC_SDK_OFF_OS_ARENA_HI, 4)) {
        g_os_arena_hi_fallback = addr;
        return;
    }
    gc_sdk_state_store_u32be(GC_SDK_OFF_OS_ARENA_HI, v);
}

// Common align helpers used throughout the SDK.
// The "32B" variants align to 32 bytes.
uint32_t OSRoundUp32B(uint32_t x) {
    return (x + 31u) & ~31u;
}

uint32_t OSRoundDown32B(uint32_t x) {
    return x & ~31u;
}
