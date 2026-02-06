#include <stdint.h>

typedef uint32_t u32;

static volatile u32 s_sel;

static void GXSetVCacheMetric(u32 attr) { s_sel = attr; }

int main(void) {
    // MP4 main.c: GXSetVCacheMetric(GX_VC_ALL);
    GXSetVCacheMetric(0x99u);

    *(volatile u32*)0x80300000 = 0xDEADBEEFu;
    *(volatile u32*)0x80300004 = (u32)s_sel;
    while (1) {}
}
