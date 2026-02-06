#include <stdint.h>
typedef uint32_t u32;

static u32 s_tv_format;
#define VI_NTSC 0u

static void VIInit(void) { s_tv_format = VI_NTSC; }
static u32 VIGetTvFormat(void) { return s_tv_format; }

int main(void) {
    VIInit();
    u32 fmt = VIGetTvFormat();

    *(volatile u32 *)0x80300000 = 0xDEADBEEFu;
    *(volatile u32 *)0x80300004 = fmt;
    while (1) {}
}
