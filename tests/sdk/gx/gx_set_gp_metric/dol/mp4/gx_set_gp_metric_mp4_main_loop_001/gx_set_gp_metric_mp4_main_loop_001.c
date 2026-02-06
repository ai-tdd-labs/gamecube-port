#include <stdint.h>

typedef uint32_t u32;

static volatile u32 s_perf0;
static volatile u32 s_perf1;

static void GXSetGPMetric(u32 perf0, u32 perf1) {
    s_perf0 = perf0;
    s_perf1 = perf1;
}

int main(void) {
    // MP4 main.c: GXSetGPMetric(GX_PERF0_CLIP_VTX, GX_PERF1_VERTICES);
    GXSetGPMetric(0x10u, 0x20u);

    *(volatile u32*)0x80300000 = 0xDEADBEEFu;
    *(volatile u32*)0x80300004 = (u32)s_perf0;
    *(volatile u32*)0x80300008 = (u32)s_perf1;
    while (1) {}
}
