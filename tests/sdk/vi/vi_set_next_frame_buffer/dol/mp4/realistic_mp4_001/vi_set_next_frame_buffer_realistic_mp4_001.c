int main(void) {
    // Realistic MP4-style: typical XFB region and a follow-up switch.
    void VISetNextFrameBuffer(void *fb) { (void)fb; }

    void *xfb1 = (void *)0x81000000;
    void *xfb2 = (void *)0x81002000;

    VISetNextFrameBuffer(xfb1);
    VISetNextFrameBuffer(xfb2);

    *(volatile unsigned int *)0x80300000 = 0xDEADBEEF;
    *(volatile unsigned int *)0x80300004 = 0x81002000;
    while (1) {}
}
