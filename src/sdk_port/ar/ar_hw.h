/*
 * sdk_port/ar/ar_hw.h --- Host-side port of GC SDK AR hardware (DMA) layer.
 *
 * Source of truth: external/mp4-decomp/src/dolphin/ar/ar.c
 *
 * Models the DSP/ARAM DMA registers as observable state.
 * ARStartDMA records DMA params; ARGetDMAStatus returns 0 (idle).
 * ARRegisterDMACallback swaps callback pointers.
 * ARSetSize is an empty stub.
 */
#pragma once

#include <stdint.h>

typedef void (*ARCallback)(void);

/* Observable DMA state (set by ARStartDMA). */
extern uint32_t gc_ar_dma_type;
extern uint32_t gc_ar_dma_mainmem;
extern uint32_t gc_ar_dma_aram;
extern uint32_t gc_ar_dma_length;
extern uint32_t gc_ar_dma_status;
extern uintptr_t gc_ar_callback_ptr;

void ARStartDMA(uint32_t type, uint32_t mainmem_addr, uint32_t aram_addr, uint32_t length);
uint32_t ARGetDMAStatus(void);
ARCallback ARRegisterDMACallback(ARCallback callback);
void ARSetSize(void);
