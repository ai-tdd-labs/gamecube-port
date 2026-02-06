#include <stdint.h>
typedef uint32_t u32;

static u32 OSRoundUp32B(u32 x) {
    return (x + 31u) & ~31u;
}

int main(void) {
    u32 out = OSRoundUp32B(0x80000001u);
    *(volatile u32*)0x80300000 = 0xDEADBEEF;
    *(volatile u32*)0x80300004 = out;
    while (1) {}
}
