#include <stdint.h>

typedef uint32_t u32;

static u32 s_bp_sent_not;

static void GXResetWriteGatherPipe(void) {
    /* Decomp behavior is PPC WPAR hardware register reset; no RAM side effects here. */
}

int main(void) {
    s_bp_sent_not = 0x11223344u;

    GXResetWriteGatherPipe();

    *(volatile u32 *)0x80300000 = 0xDEADBEEFu;
    *(volatile u32 *)0x80300004 = s_bp_sent_not;
    while (1) {}
}
