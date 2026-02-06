#include <stdint.h>

typedef uint32_t u32;

static volatile u32 s_perf0 = 0x11111111u;
static volatile u32 s_perf1 = 0x22222222u;

static void GXReadGPMetric(u32 *met0, u32 *met1) {
    if (met0) *met0 = s_perf0;
    if (met1) *met1 = s_perf1;
}

int main(void) {
    u32 a = 0, b = 0;
    GXReadGPMetric(&a, &b);

    *(volatile u32*)0x80300000 = 0xDEADBEEFu;
    *(volatile u32*)0x80300004 = a;
    *(volatile u32*)0x80300008 = b;
    while (1) {}
}
