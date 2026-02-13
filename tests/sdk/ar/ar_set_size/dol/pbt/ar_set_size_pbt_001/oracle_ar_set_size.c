#include <stdint.h>

typedef uint32_t u32;

u32 gc_ar_dma_type;
u32 gc_ar_dma_mainmem;
u32 gc_ar_dma_aram;
u32 gc_ar_dma_length;
u32 gc_ar_dma_status;
uintptr_t gc_ar_callback_ptr;

void oracle_ARSetSize(void) {
    /* No-op per decomp. */
}

