#include <stdint.h>
typedef uint32_t u32;

static u32 OSGetPhysicalMemSize(void) { return 0x01800000u; }

int main(void) {
    u32 v = OSGetPhysicalMemSize();
    *(volatile u32*)0x80300000 = 0xDEADBEEFu;
    *(volatile u32*)0x80300004 = v;
    while (1) {}
}
