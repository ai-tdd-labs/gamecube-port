#include <stdint.h>

typedef uint32_t u32;

static volatile u32 s_perf0 = 0xAAAAAAAAu;
static volatile u32 s_perf1 = 0xBBBBBBBBu;

static void GXClearGPMetric(void) {
    s_perf0 = 0;
    s_perf1 = 0;
}

int main(void) {
    GXClearGPMetric();

    *(volatile u32*)0x80300000 = 0xDEADBEEFu;
    *(volatile u32*)0x80300004 = (u32)s_perf0;
    *(volatile u32*)0x80300008 = (u32)s_perf1;
    while (1) {}
}
