#include <stdint.h>
typedef uint32_t u32;

static void OSInitFastCast(void) {
    // Synthetic no-op. Real SDK is MW-only inline that programs GQR2-5.
}

int main(void) {
    OSInitFastCast();

    *(volatile u32 *)0x80300000 = 0xDEADBEEFu;
    *(volatile u32 *)0x80300004 = 0u;
    while (1) {}
}
