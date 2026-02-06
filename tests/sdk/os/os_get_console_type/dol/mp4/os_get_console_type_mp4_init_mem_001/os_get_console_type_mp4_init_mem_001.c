#include <stdint.h>
typedef uint32_t u32;

static u32 OSGetConsoleType(void) { return 0; }

int main(void) {
    u32 v = OSGetConsoleType();
    *(volatile u32*)0x80300000 = 0xDEADBEEFu;
    *(volatile u32*)0x80300004 = v;
    while (1) {}
}
