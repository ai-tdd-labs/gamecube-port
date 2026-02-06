#include <stdint.h>

// Minimal SDK port for Arena pointers.
// Kept intentionally small: tests drive behavior, no refactors.

static void *g_os_arena_lo = (void *)(uintptr_t)-1;
static void *g_os_arena_hi = 0;

void *OSGetArenaLo(void) {
    return g_os_arena_lo;
}

void *OSGetArenaHi(void) {
    return g_os_arena_hi;
}

void OSSetArenaLo(void *addr) {
    g_os_arena_lo = addr;
}

void OSSetArenaHi(void *addr) {
    g_os_arena_hi = addr;
}

// Common align helpers used throughout the SDK.
// The "32B" variants align to 32 bytes.
uint32_t OSRoundUp32B(uint32_t x) {
    return (x + 31u) & ~31u;
}

uint32_t OSRoundDown32B(uint32_t x) {
    return x & ~31u;
}
