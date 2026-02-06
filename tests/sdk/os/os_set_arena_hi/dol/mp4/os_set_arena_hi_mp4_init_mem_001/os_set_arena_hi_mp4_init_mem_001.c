#include <stdint.h>
typedef uint32_t u32;

static void *s_arena_hi;
static void OSSetArenaHi(void *addr) { s_arena_hi = addr; }
static void *OSGetArenaHi(void) { return s_arena_hi; }

int main(void) {
    OSSetArenaHi((void*)0x81654320u);

    *(volatile u32*)0x80300000 = 0xDEADBEEFu;
    *(volatile u32*)0x80300004 = (u32)(uintptr_t)OSGetArenaHi();
    while (1) {}
}
