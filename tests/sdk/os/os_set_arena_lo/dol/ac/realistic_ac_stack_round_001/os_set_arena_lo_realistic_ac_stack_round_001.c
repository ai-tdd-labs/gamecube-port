#include <dolphin/types.h>
#include <dolphin/os/OSArena.h>

#include "decomp_mario_party_4/src/dolphin/os/OSArena.c"

int main(void) {
    // Realistic AC-style pattern: stack rounding to 0x20 and store it.
    // Evidence: decomp_animal_crossing/src/static/dolphin/os/OS.c sets:
    //   OSSetArenaLo((void*)(((u32)(char*)&_stack_addr + 0x1F) & 0xFFFFFFE0));
    //
    // In the real SDK, `_stack_addr` is a linker symbol. For deterministic unit tests we
    // use a fixed representative address and apply the exact same rounding formula.
    u32 stack_addr = 0x81234523;
    u32 v = (stack_addr + 0x1F) & 0xFFFFFFE0;
    OSSetArenaLo((void*)v);

    *(volatile u32*)0x80300000 = 0xDEADBEEF;
    *(volatile u32*)0x80300004 = (u32)OSGetArenaLo();
    while (1) {}
}
