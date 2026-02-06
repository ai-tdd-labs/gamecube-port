#include <stdint.h>
typedef uint32_t u32;

static volatile u32 s_calls;
static void VIWaitForRetrace(void) { s_calls++; }

int main(void) {
    VIWaitForRetrace();
    VIWaitForRetrace();

    *(volatile u32*)0x80300000 = 0xDEADBEEFu;
    *(volatile u32*)0x80300004 = s_calls;
    while (1) {}
}
