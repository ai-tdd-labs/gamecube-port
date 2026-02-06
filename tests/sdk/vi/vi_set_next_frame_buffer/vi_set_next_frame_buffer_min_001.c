#include <dolphin/types.h>
#include <dolphin/vi.h>

int main(void) {
    // Minimal call: aligned pointer.
    void *xfb = (void *)0x81000000;
    VISetNextFrameBuffer(xfb);

    *(volatile u32 *)0x80300000 = 0xDEADBEEF;
    *(volatile u32 *)0x80300004 = 0x81000000;
    while (1) {}
}
