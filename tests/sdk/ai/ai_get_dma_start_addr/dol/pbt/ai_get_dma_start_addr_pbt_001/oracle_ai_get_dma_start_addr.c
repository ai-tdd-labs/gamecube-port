#include <stdint.h>

uint16_t gc_ai_dsp_regs[4];

uint32_t oracle_AIGetDMAStartAddr(void)
{
    return (uint32_t)((gc_ai_dsp_regs[0] & 0x03FFu) << 16) | (gc_ai_dsp_regs[1] & 0xFFE0u);
}

