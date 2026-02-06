#include <stdint.h>
typedef uint32_t u32;

static volatile u32 s_black;
static void VISetBlack(u32 black) { s_black = black ? 1u : 0u; }

int main(void) {
    VISetBlack(0);

    *(volatile u32*)0x80300000 = 0xDEADBEEFu;
    *(volatile u32*)0x80300004 = s_black;
    while (1) {}
}
