#include <stdint.h>

// Minimal OSInit used by host scenarios.
// This is not a full OSInit port (yet); it's only enough for deterministic tests.

void OSSetArenaLo(void *addr);
void OSSetArenaHi(void *addr);

void OSInit(void) {
    // Conservative defaults (match what our synthetic tests expect).
    OSSetArenaLo((void *)0x80002000u);
    OSSetArenaHi((void *)0x81700000u);
}

