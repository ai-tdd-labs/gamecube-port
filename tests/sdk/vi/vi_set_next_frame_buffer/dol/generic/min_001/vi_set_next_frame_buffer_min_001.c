int main(void) {
    // Minimal local stub for Dolphin expected.bin generation.
    // We are not validating VI internals here, only call/flow.
    void VISetNextFrameBuffer(void *fb) { (void)fb; }

    void *xfb = (void *)0x81000000;
    VISetNextFrameBuffer(xfb);

    *(volatile unsigned int *)0x80300000 = 0xDEADBEEF;
    *(volatile unsigned int *)0x80300004 = 0x81000000;
    while (1) {}
}
