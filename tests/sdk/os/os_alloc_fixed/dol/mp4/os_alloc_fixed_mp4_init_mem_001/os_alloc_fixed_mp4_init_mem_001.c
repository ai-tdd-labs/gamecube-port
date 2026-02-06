#include <stdint.h>
typedef uint32_t u32;

static volatile u32 s_calls;
static volatile u32 s_last;
static void *OSAllocFixed(u32 size) { s_calls++; s_last = size; return (void*)0x81234000u; }

int main(void) {
    (void)OSAllocFixed(0x40);

    *(volatile u32*)0x80300000 = 0xDEADBEEFu;
    *(volatile u32*)0x80300004 = s_calls;
    *(volatile u32*)0x80300008 = s_last;
    while (1) {}
}
