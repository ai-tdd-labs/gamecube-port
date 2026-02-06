#include <stdint.h>
typedef uint32_t u32;

static volatile u32 s_gamma;

static void GXSetDispCopyGamma(u32 gamma) {
    s_gamma = gamma;
}

int main(void) {
    // MP4 init.c: GXSetDispCopyGamma(GX_GM_1_0)
    GXSetDispCopyGamma(0);

    *(volatile u32 *)0x80300000 = 0xDEADBEEFu;
    *(volatile u32 *)0x80300004 = s_gamma;
    while (1) {}
}
