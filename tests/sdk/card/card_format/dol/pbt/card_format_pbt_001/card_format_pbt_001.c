#include <stdint.h>
#include "src/sdk_port/gc_mem.c"

typedef uint8_t u8;
typedef uint32_t u32;

typedef int32_t s32;

void oracle_card_format_pbt_001_suite(u8* out);

int main(void)
{
    gc_mem_set(0x80000000u, 0x01800000u, (void*)0x80000000u);

    volatile u8* out = (volatile u8*)0x80300000u;
    for (u32 i = 0; i < 0x200u; i++) {
        out[i] = 0;
    }

    oracle_card_format_pbt_001_suite((u8*)out);

    while (1) {
        __asm__ volatile("nop");
    }

    return (s32)sizeof(u32);
}
