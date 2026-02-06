int main(void) {
    // Edge case: unaligned pointer. Dolphin behavior is oracle.
    // Write a pre-marker so we can detect an early assert/crash vs a completed call.
    void VISetNextFrameBuffer(void *fb) { (void)fb; }

    *(volatile unsigned int *)0x80300000 = 0x11111111;

    void *xfb = (void *)0x81000004;
    VISetNextFrameBuffer(xfb);

    *(volatile unsigned int *)0x80300000 = 0x22222222;
    *(volatile unsigned int *)0x80300004 = 0x81000004;
    while (1) {}
}
