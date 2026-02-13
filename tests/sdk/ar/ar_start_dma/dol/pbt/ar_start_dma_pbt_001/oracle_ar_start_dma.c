#include <stdint.h>

typedef uint32_t u32;

u32 gc_ar_dma_type;
u32 gc_ar_dma_mainmem;
u32 gc_ar_dma_aram;
u32 gc_ar_dma_length;

void oracle_ARStartDMA_reset(u32 s) {
    gc_ar_dma_type = 0x10000000u ^ s;
    gc_ar_dma_mainmem = 0x20000000u ^ (s * 3u);
    gc_ar_dma_aram = 0x30000000u ^ (s * 5u);
    gc_ar_dma_length = 0x40000000u ^ (s * 7u);
}

void oracle_ARStartDMA(u32 type, u32 mainmem_addr, u32 aram_addr, u32 length) {
    gc_ar_dma_type = type;
    gc_ar_dma_mainmem = mainmem_addr;
    gc_ar_dma_aram = aram_addr;
    gc_ar_dma_length = length;
}
