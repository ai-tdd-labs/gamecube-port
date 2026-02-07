#include <stdint.h>

typedef uint32_t u32;

typedef struct {
    u32 enable;
    u32 alpha;
} Out;

static volatile Out g_out;

static void GXSetDstAlpha(u32 enable, u32 alpha) {
    g_out.enable = enable;
    g_out.alpha = alpha;
}

int main(void) {
    // From GXInit.c: GXSetDstAlpha(GX_DISABLE, 0)
    GXSetDstAlpha(0, 0);

    *(volatile u32 *)0x80300000 = 0xDEADBEEFu;
    *(volatile u32 *)0x80300004 = g_out.enable;
    *(volatile u32 *)0x80300008 = g_out.alpha;
    while (1) {}
}
