#include <stdint.h>
typedef uint32_t u32;
typedef uint8_t u8;

static volatile u32 s_enable;
static volatile u32 s_func;
static volatile u32 s_update;

static void GXSetZMode(u8 enable, u32 func, u8 update_enable) {
    s_enable = enable;
    s_func = func;
    s_update = update_enable;
}

int main(void) {
    // MP4 HuSysDoneRender uses GXSetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE)
    GXSetZMode(1, 3, 1);

    *(volatile u32*)0x80300000 = 0xDEADBEEFu;
    *(volatile u32*)0x80300004 = s_enable;
    *(volatile u32*)0x80300008 = s_func;
    *(volatile u32*)0x8030000C = s_update;
    while (1) {}
}
