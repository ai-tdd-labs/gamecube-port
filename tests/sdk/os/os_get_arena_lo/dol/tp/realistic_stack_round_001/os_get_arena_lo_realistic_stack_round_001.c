#include <dolphin/types.h>
#include <dolphin/os/OSArena.h>

#include "decomp_mario_party_4/src/dolphin/os/OSArena.c"

static u32 round_up_0x20(u32 v) { return (v + 0x1F) & 0xFFFFFFE0; }

int main(void) {
    // Realistic TP-style: arenaLo may be set to a 0x20-rounded stack address.
    u32 stack_addr = 0x81234523;
    OSSetArenaLo((void*)round_up_0x20(stack_addr));

    *(volatile u32*)0x80300000 = 0xDEADBEEF;
    *(volatile u32*)0x80300004 = (u32)OSGetArenaLo();
    while (1) {}
}

