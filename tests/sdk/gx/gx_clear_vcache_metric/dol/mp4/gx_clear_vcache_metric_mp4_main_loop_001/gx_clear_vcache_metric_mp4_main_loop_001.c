#include <stdint.h>

typedef uint32_t u32;

static volatile u32 s_sel = 0x77777777u;

static void GXClearVCacheMetric(void) { s_sel = 0; }

int main(void) {
    GXClearVCacheMetric();

    *(volatile u32*)0x80300000 = 0xDEADBEEFu;
    *(volatile u32*)0x80300004 = (u32)s_sel;
    while (1) {}
}
