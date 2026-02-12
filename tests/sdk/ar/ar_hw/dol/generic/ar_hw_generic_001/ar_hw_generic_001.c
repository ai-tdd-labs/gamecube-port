#include <stdint.h>

typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t  u8;

typedef void (*ARCallback)(void);

void ARStartDMA(u32 type, u32 mainmem_addr, u32 aram_addr, u32 length);
u32 ARGetDMAStatus(void);
ARCallback ARRegisterDMACallback(ARCallback callback);
void ARSetSize(void);

extern u32 gc_ar_dma_type;
extern u32 gc_ar_dma_mainmem;
extern u32 gc_ar_dma_aram;
extern u32 gc_ar_dma_length;
extern u32 gc_ar_dma_status;

static inline void wr32be(volatile uint8_t *p, u32 v) {
    p[0]=(uint8_t)(v>>24); p[1]=(uint8_t)(v>>16);
    p[2]=(uint8_t)(v>>8);  p[3]=(uint8_t)(v);
}

static void dummy_callback(void) {}

int main(void) {
    volatile uint8_t *out = (volatile uint8_t *)0x80300000;
    for (u32 i = 0; i < 0x40; i++) out[i] = 0;

    // Test ARSetSize (empty stub — just call it).
    ARSetSize();

    // Test ARRegisterDMACallback: set a callback, then replace it.
    ARCallback old1 = ARRegisterDMACallback(dummy_callback);
    ARCallback old2 = ARRegisterDMACallback((ARCallback)0);

    // Test ARGetDMAStatus before any DMA.
    u32 status_before = ARGetDMAStatus();

    // Test ARStartDMA: MRAM->ARAM transfer.
    ARStartDMA(0, 0x80100000u, 0x00004000u, 0x00001000u);

    // Test ARGetDMAStatus after DMA (still 0 in port).
    u32 status_after = ARGetDMAStatus();

    // Test ARStartDMA again: ARAM->MRAM transfer.
    ARStartDMA(1, 0x80200000u, 0x00008000u, 0x00002000u);

    u32 off = 0;
    wr32be(out + off, 0xDEADBEEFu); off += 4;
    wr32be(out + off, (u32)(old1 == 0 ? 0 : 1)); off += 4;
    wr32be(out + off, (u32)(old2 != 0 ? 1 : 0)); off += 4;
    wr32be(out + off, status_before); off += 4;
    wr32be(out + off, status_after); off += 4;
    wr32be(out + off, gc_ar_dma_type); off += 4;
    wr32be(out + off, gc_ar_dma_mainmem); off += 4;
    wr32be(out + off, gc_ar_dma_aram); off += 4;
    wr32be(out + off, gc_ar_dma_length); off += 4;
    // Total: 9 * 4 = 0x24

    while (1) { __asm__ volatile("nop"); }
    return 0;
}
