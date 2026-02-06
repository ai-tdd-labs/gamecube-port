#include <dolphin/types.h>
#include <dolphin/os/OSArena.h>

#include "decomp_mario_party_4/src/dolphin/os/OSArena.c"

int main(void) {
    // Realistic WW-style: mDoMch_Create computes arena size and may shrink ArenaHi.
    // Reference callsite:
    //   decomp_wind_waker/src/m_Do/m_Do_machine.cpp:mDoMch_Create()
    //
    // We pick deterministic arena values that exercise the "shrink ArenaHi" branch.
    OSSetArenaLo((void*)0x80000000);
    OSSetArenaHi((void*)0x83000000);

    u32 arenaHi = (u32)OSGetArenaHi();
    u32 arenaLo = (u32)OSGetArenaLo();
    if (arenaHi > 0x81800000 && arenaHi - 0x1800000 > arenaLo) {
        OSSetArenaHi((void*)(arenaHi - 0x1800000));
    }

    u32 arenaSize = ((u32)OSGetArenaHi() - (u32)OSGetArenaLo()) - 0xF0;

    *(volatile u32*)0x80300000 = 0xDEADBEEF;
    *(volatile u32*)0x80300004 = (u32)OSGetArenaLo();
    *(volatile u32*)0x80300008 = (u32)OSGetArenaHi();
    *(volatile u32*)0x8030000C = arenaSize;
    while (1) {}
}
