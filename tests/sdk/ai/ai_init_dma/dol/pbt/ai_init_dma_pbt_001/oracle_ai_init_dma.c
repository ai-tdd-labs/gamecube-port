#include <stdint.h>

uint16_t gc_ai_dsp_regs[4];

void oracle_AIInitDMA(uint32_t addr, uint32_t length)
{
    gc_ai_dsp_regs[0] = (uint16_t)((gc_ai_dsp_regs[0] & ~0x03FFu) | (addr >> 16));
    gc_ai_dsp_regs[1] = (uint16_t)((gc_ai_dsp_regs[1] & ~0xFFE0u) | (uint16_t)(addr & 0xFFFFu));
    gc_ai_dsp_regs[3] = (uint16_t)((gc_ai_dsp_regs[3] & ~0x7FFFu) | (uint16_t)((length >> 5) & 0xFFFFu));
}

