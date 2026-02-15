#include <stdint.h>

#include "src/sdk_port/gc_mem.c"

void oracle_card_check_pbt_001_suite(uint8_t* out);

int main(void)
{
    gc_mem_set(0x80000000u, 0x01800000u, (uint8_t*)0x80000000u);

    volatile uint8_t* out = (uint8_t*)0x80300000u;
    for (uint32_t i = 0; i < 0x200u; i++) {
        out[i] = 0;
    }

    oracle_card_check_pbt_001_suite((uint8_t*)out);

    while (1) {
        __asm__ volatile("nop");
    }
    return 0;
}
