#include <stdint.h>
typedef uint32_t u32;

static u32 OSRoundDown32B(u32 x) { return x & ~31u; }

int main(void) {
    u32 out = OSRoundDown32B(0x8000003Fu);
    *(volatile u32*)0x80300000 = 0xDEADBEEFu;
    *(volatile u32*)0x80300004 = out;
    while (1) {}
}
