#include <dolphin/types.h>
#include <dolphin/os/OSArena.h>

// Build OSArena.c into this DOL so we test real SDK code from decomp.
// (No host/runtime port involved yet; this is for Dolphin expected.bin.)
#include "decomp_mario_party_4/src/dolphin/os/OSArena.c"

int main(void) {
    OSSetArenaLo((void*)0x81234560);

    *(volatile u32*)0x80300000 = 0xDEADBEEF;
    *(volatile u32*)0x80300004 = (u32)OSGetArenaLo();
    while (1) {}
}
