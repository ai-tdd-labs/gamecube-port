#include <stdint.h>
typedef uint32_t u32;
typedef uint8_t u8;

static volatile u32 s_dest;
static volatile u32 s_clear;

static void GXCopyDisp(void *dest, u8 clear) {
    s_dest = (u32)(uintptr_t)dest;
    s_clear = (u32)clear;
}

int main(void) {
    // MP4 init.c: GXCopyDisp(DemoCurrentBuffer, GX_TRUE)
    GXCopyDisp((void*)0x81234000u, 1);

    *(volatile u32 *)0x80300000 = 0xDEADBEEFu;
    *(volatile u32 *)0x80300004 = s_dest;
    *(volatile u32 *)0x80300008 = s_clear;
    while (1) {}
}
