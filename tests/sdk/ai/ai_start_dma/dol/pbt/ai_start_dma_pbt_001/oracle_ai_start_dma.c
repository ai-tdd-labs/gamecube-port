#include <stdint.h>

uint16_t gc_ai_dsp_regs[4];

void oracle_AIStartDMA(void)
{
    gc_ai_dsp_regs[3] |= 0x8000u;
}

