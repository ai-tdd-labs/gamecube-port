#include <dolphin/types.h>
#include <stdint.h>

// Use the decomp SDK implementation as the Dolphin oracle.
#include "decomp_mario_party_4/src/dolphin/os/OSArena.c"

static void write_result(u32 marker, u32 hi) {
    volatile u32 *out = (volatile u32 *)0x80300000;
    out[0] = marker;
    out[1] = hi;
}

int main(void) {
    OSSetArenaHi((void *)0x80010000u);
    OSSetArenaHi((void *)0x81700000u);
    void *hi = OSGetArenaHi();

    write_result(0xDEADBEEFu, (u32)(uintptr_t)hi);
    while (1) {}
}
