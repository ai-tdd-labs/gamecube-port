#include <stdint.h>
typedef uint32_t u32;

static volatile u32 s_pix_fmt;
static volatile u32 s_z_fmt;

static void GXSetPixelFmt(u32 pix_fmt, u32 z_fmt) {
    s_pix_fmt = pix_fmt;
    s_z_fmt = z_fmt;
}

int main(void) {
    // MP4 init.c chooses between 2 pixel formats based on RenderMode->aa.
    // Exercise the aa==0 path: GX_PF_RGB8_Z24, GX_ZC_LINEAR.
    GXSetPixelFmt(2, 1);

    *(volatile u32 *)0x80300000 = 0xDEADBEEFu;
    *(volatile u32 *)0x80300004 = s_pix_fmt;
    *(volatile u32 *)0x80300008 = s_z_fmt;
    while (1) {}
}
