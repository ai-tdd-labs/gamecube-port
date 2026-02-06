#include <dolphin/types.h>
#include <dolphin/os/OSArena.h>

#include "decomp_mario_party_4/src/dolphin/os/OSArena.c"

int main(void) {
    // Realistic fixed-value callsite style (seen in TP OSExec/OSReboot).
    OSSetArenaLo((void*)0x81280000);

    *(volatile u32*)0x80300000 = 0xDEADBEEF;
    *(volatile u32*)0x80300004 = (u32)OSGetArenaLo();
    while (1) {}
}
