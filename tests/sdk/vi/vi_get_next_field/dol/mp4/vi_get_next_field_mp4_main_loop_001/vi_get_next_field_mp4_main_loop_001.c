#include <stdint.h>
typedef uint32_t u32;

static volatile u32 s_field;
static u32 VIGetNextField(void) { return s_field; }

int main(void) {
    s_field = 0;
    u32 v = VIGetNextField();

    *(volatile u32*)0x80300000 = 0xDEADBEEFu;
    *(volatile u32*)0x80300004 = v;
    while (1) {}
}
