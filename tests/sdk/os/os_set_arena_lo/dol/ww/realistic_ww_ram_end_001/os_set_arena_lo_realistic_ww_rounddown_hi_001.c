#include <dolphin/types.h>
#include <dolphin/os/OSArena.h>

#include "decomp_mario_party_4/src/dolphin/os/OSArena.c"

static u32 round_down_0x20(u32 v) { return v & 0xFFFFFFE0; }

int main(void) {
    // Realistic WW-style pattern: compute a "ram_end"-like value (0x20-aligned) and store it.
    // Evidence: decomp_wind_waker/src/JSystem/JKernel/JKRHeap.cpp calls OSSetArenaLo(ram_end).
    u32 arena_hi = 0x81800013;
    u32 ram_end = round_down_0x20(arena_hi);

    OSSetArenaLo((void*)ram_end);

    *(volatile u32*)0x80300000 = 0xDEADBEEF;
    *(volatile u32*)0x80300004 = (u32)OSGetArenaLo();
    while (1) {}
}
