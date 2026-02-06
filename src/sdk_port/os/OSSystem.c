#include <stdint.h>

// Minimal system queries used by early init code paths.
// Deterministic constants are fine as long as tests assert them and higher-level
// behavior is validated by smoke/chain tests.

uint32_t OSGetConsoleType(void) {
    // Retail-like default (not DEVHW1).
    return 0;
}

uint32_t OSGetPhysicalMemSize(void) {
    // Chosen to avoid the special-case in MP4 InitMem.
    return 0x01800000u;
}

uint32_t OSGetConsoleSimulatedMemSize(void) {
    return 0x01800000u;
}

