#include <dolphin/types.h>
#include <dolphin/os/OSArena.h>

#include "decomp_mario_party_4/src/dolphin/os/OSArena.c"

int main(void) {
    // Realistic AC-style: boot code reads arena lo/hi and derives heap sizes.
    // Reference callsite:
    //   decomp_animal_crossing/src/static/jsyswrap.cpp:JW_Init()
    //
    // SystemHeapSize = arena_hi - arena_lo - 0xD0
    OSSetArenaLo((void*)0x80400000);
    OSSetArenaHi((void*)0x81800000);

    u32 arenaHi = (u32)OSGetArenaHi();
    u32 arenaLo = (u32)OSGetArenaLo();
    u32 systemHeapSize = arenaHi - arenaLo - 0xD0;

    *(volatile u32*)0x80300000 = 0xDEADBEEF;
    *(volatile u32*)0x80300004 = arenaLo;
    *(volatile u32*)0x80300008 = arenaHi;
    *(volatile u32*)0x8030000C = systemHeapSize;
    while (1) {}
}
