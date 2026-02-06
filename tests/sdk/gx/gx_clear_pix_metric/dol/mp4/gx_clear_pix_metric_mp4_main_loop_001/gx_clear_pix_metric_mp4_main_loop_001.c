#include <stdint.h>

typedef uint32_t u32;

static volatile u32 s_pix[6] = {1,2,3,4,5,6};

static void GXClearPixMetric(void) {
    u32 i;
    for (i = 0; i < 6; i++) s_pix[i] = 0;
}

int main(void) {
    u32 i;
    GXClearPixMetric();

    *(volatile u32*)0x80300000 = 0xDEADBEEFu;
    for (i = 0; i < 6; i++) {
        *(volatile u32*)(0x80300004u + i*4u) = (u32)s_pix[i];
    }
    while (1) {}
}
