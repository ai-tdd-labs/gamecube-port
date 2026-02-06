#include <stdint.h>

typedef uint32_t u32;

static volatile u32 s_sel = 0x12345678u;

static void GXReadVCacheMetric(u32 *check, u32 *miss, u32 *stall) {
    if (check) *check = s_sel;
    if (miss) *miss = 0;
    if (stall) *stall = 0;
}

int main(void) {
    u32 a = 0, b = 0, c = 0;
    GXReadVCacheMetric(&a, &b, &c);

    *(volatile u32*)0x80300000 = 0xDEADBEEFu;
    *(volatile u32*)0x80300004 = a;
    *(volatile u32*)0x80300008 = b;
    *(volatile u32*)0x8030000C = c;
    while (1) {}
}
