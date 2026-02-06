#include <dolphin/types.h>
#include <dolphin/os/OSArena.h>

#include "decomp_mario_party_4/src/dolphin/os/OSArena.c"

int main(void) {
    // Realistic MP4-style: GCN_Mem_Alloc reads arenaLo early.
    // We simulate that arenaLo has been set during OS init / boot.
    OSSetArenaLo((void*)0x81000000);

    *(volatile u32*)0x80300000 = 0xDEADBEEF;
    *(volatile u32*)0x80300004 = (u32)OSGetArenaLo();
    while (1) {}
}

