#include <stdint.h>
typedef uint32_t u32;
typedef uint8_t u8;

static volatile u32 s_enable;
static void GXSetColorUpdate(u8 enable) { s_enable = enable; }

int main(void) {
    // MP4 HuSysDoneRender uses GXSetColorUpdate(GX_TRUE)
    GXSetColorUpdate(1);

    *(volatile u32*)0x80300000 = 0xDEADBEEFu;
    *(volatile u32*)0x80300004 = s_enable;
    while (1) {}
}
