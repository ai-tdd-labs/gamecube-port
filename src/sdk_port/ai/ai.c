/*
 * sdk_port/ai/ai.c --- Host-side port of GC SDK AI (audio interface).
 *
 * Source of truth: external/mp4-decomp/src/dolphin/ai.c
 *
 * Real SDK writes to __AIRegs (0xCC006C00) and __DSPRegs (0xCC005000).
 * We model these as observable arrays for trace test parity.
 * __AI_SRC_INIT (hardware timing calibration loop) is a no-op.
 */
#include <stdint.h>
#include "ai.h"

uint32_t gc_ai_regs[4];
uint16_t gc_ai_dsp_regs[4];
uintptr_t gc_ai_dma_cb_ptr;

/* AIRegisterDMACallback (ai.c:25-35) */
AIDCallback AIRegisterDMACallback(AIDCallback callback)
{
    AIDCallback old = (AIDCallback)gc_ai_dma_cb_ptr;
    gc_ai_dma_cb_ptr = (uintptr_t)callback;
    return old;
}

/* AIInitDMA (ai.c:37-45) — Set DMA start address and length via DSP regs. */
void AIInitDMA(uint32_t addr, uint32_t length)
{
    gc_ai_dsp_regs[0] = (uint16_t)((gc_ai_dsp_regs[0] & ~0x3FF) | (addr >> 16));
    gc_ai_dsp_regs[1] = (uint16_t)((gc_ai_dsp_regs[1] & ~0xFFE0) | (0xFFFF & addr));
    gc_ai_dsp_regs[3] = (uint16_t)((gc_ai_dsp_regs[3] & ~0x7FFF) | (uint16_t)((length >> 5) & 0xFFFF));
}

/* AIStartDMA (ai.c:47-50) — Set DMA start bit. */
void AIStartDMA(void)
{
    gc_ai_dsp_regs[3] |= 0x8000;
}

/* AIGetDMAStartAddr (ai.c:57-60) — Read DMA start address from DSP regs. */
uint32_t AIGetDMAStartAddr(void)
{
    return (uint32_t)((gc_ai_dsp_regs[0] & 0x03FF) << 16) | (gc_ai_dsp_regs[1] & 0xFFE0);
}

/* AISetStreamPlayState (ai.c:84-109) — Set stream play bit in AI control reg. */
void AISetStreamPlayState(uint32_t state)
{
    if (state == AIGetStreamPlayState()) {
        return;
    }
    if ((AIGetStreamSampleRate() == 0u) && (state == 1)) {
        uint8_t volRight = AIGetStreamVolRight();
        uint8_t volLeft = AIGetStreamVolLeft();
        AISetStreamVolRight(0);
        AISetStreamVolLeft(0);
        /* __AI_SRC_INIT() — hardware timing calibration, no-op in port. */
        gc_ai_regs[0] = (gc_ai_regs[0] & ~0x20u) | 0x20u;
        gc_ai_regs[0] = (gc_ai_regs[0] & ~1u) | 1u;
        AISetStreamVolLeft(volRight);
        AISetStreamVolRight(volLeft);
    } else {
        gc_ai_regs[0] = (gc_ai_regs[0] & ~1u) | state;
    }
}

/* AIGetStreamPlayState (ai.c:111-114) */
uint32_t AIGetStreamPlayState(void)
{
    return gc_ai_regs[0] & 1u;
}

/* AISetStreamVolLeft (ai.c:187-190) */
void AISetStreamVolLeft(uint8_t volume)
{
    gc_ai_regs[1] = (gc_ai_regs[1] & ~0xFFu) | ((uint32_t)volume & 0xFFu);
}

/* AIGetStreamVolLeft (ai.c:192-195) */
uint8_t AIGetStreamVolLeft(void)
{
    return (uint8_t)gc_ai_regs[1];
}

/* AISetStreamVolRight (ai.c:197-200) */
void AISetStreamVolRight(uint8_t volume)
{
    gc_ai_regs[1] = (gc_ai_regs[1] & ~0xFF00u) | (((uint32_t)volume & 0xFFu) << 8);
}

/* AIGetStreamVolRight (ai.c:202-205) */
uint8_t AIGetStreamVolRight(void)
{
    return (uint8_t)(gc_ai_regs[1] >> 8);
}

/* AIGetStreamSampleRate (ai.c:182-185) */
uint32_t AIGetStreamSampleRate(void)
{
    return (gc_ai_regs[0] >> 1) & 1u;
}
