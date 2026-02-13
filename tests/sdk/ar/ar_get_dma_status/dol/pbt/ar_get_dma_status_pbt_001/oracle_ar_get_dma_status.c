#include <stdint.h>

typedef uint32_t u32;

u32 gc_ar_dma_status;

void oracle_ARGetDMAStatus_reset(u32 s) {
    gc_ar_dma_status = 0xA5A50000u ^ s;
}

u32 oracle_ARGetDMAStatus(void) {
    return gc_ar_dma_status;
}
