#include <stdint.h>

typedef void (*AIDCallback)(void);

uintptr_t gc_ai_dma_cb_ptr;

void oracle_AIRegisterDMACallback_reset(uint32_t s)
{
    gc_ai_dma_cb_ptr = (uintptr_t)(0x80000000u | (s & 0x00FFFFFFu));
}

AIDCallback oracle_AIRegisterDMACallback(AIDCallback callback)
{
    AIDCallback old = (AIDCallback)gc_ai_dma_cb_ptr;
    gc_ai_dma_cb_ptr = (uintptr_t)callback;
    return old;
}

