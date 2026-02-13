#include <stdint.h>

uint32_t gc_ai_regs[4];

void oracle_AISetStreamVolLeft(uint8_t volume)
{
    gc_ai_regs[1] = (gc_ai_regs[1] & ~0xFFu) | ((uint32_t)volume & 0xFFu);
}

uint8_t oracle_AIGetStreamVolLeft(void)
{
    return (uint8_t)gc_ai_regs[1];
}

uint8_t oracle_AIGetStreamVolRight(void)
{
    return (uint8_t)(gc_ai_regs[1] >> 8);
}

