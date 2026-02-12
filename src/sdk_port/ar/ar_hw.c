/*
 * sdk_port/ar/ar_hw.c --- Host-side port of GC SDK AR hardware (DMA) layer.
 *
 * Source of truth: external/mp4-decomp/src/dolphin/ar/ar.c
 *
 * The real SDK writes to __DSPRegs[16..21] for DMA and reads __DSPRegs[5]
 * for status. We model these as observable globals for trace test parity.
 */
#include <stdint.h>
#include "ar_hw.h"

uint32_t gc_ar_dma_type;
uint32_t gc_ar_dma_mainmem;
uint32_t gc_ar_dma_aram;
uint32_t gc_ar_dma_length;
uint32_t gc_ar_dma_status;
uintptr_t gc_ar_callback_ptr;

/* ARStartDMA (ar.c:45-59) — Record DMA parameters as observable state. */
void ARStartDMA(uint32_t type, uint32_t mainmem_addr, uint32_t aram_addr, uint32_t length)
{
    gc_ar_dma_type = type;
    gc_ar_dma_mainmem = mainmem_addr;
    gc_ar_dma_aram = aram_addr;
    gc_ar_dma_length = length;
}

/* ARGetDMAStatus (ar.c:35-43) — Return 0 (idle). Real HW reads __DSPRegs[5] bit 9. */
uint32_t ARGetDMAStatus(void)
{
    return gc_ar_dma_status;
}

/* ARRegisterDMACallback (ar.c:24-33) — Swap callback pointer. */
ARCallback ARRegisterDMACallback(ARCallback callback)
{
    ARCallback old = (ARCallback)gc_ar_callback_ptr;
    gc_ar_callback_ptr = (uintptr_t)callback;
    return old;
}

/* ARSetSize (ar.c:137) — Empty stub per decomp. */
void ARSetSize(void)
{
}
