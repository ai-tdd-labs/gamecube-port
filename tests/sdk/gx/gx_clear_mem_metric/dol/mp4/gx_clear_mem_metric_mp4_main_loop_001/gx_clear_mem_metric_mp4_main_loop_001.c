#include <stdint.h>

typedef uint32_t u32;

static volatile u32 s_mem[10] = {1,2,3,4,5,6,7,8,9,10};

static void GXClearMemMetric(void) {
    u32 i;
    for (i = 0; i < 10; i++) s_mem[i] = 0;
}

int main(void) {
    u32 i;
    GXClearMemMetric();

    *(volatile u32*)0x80300000 = 0xDEADBEEFu;
    for (i = 0; i < 10; i++) {
        *(volatile u32*)(0x80300004u + i*4u) = (u32)s_mem[i];
    }
    while (1) {}
}
