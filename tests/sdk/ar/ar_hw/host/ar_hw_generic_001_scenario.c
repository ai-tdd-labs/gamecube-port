#include <stdint.h>
#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

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

static void dummy_callback(void) {}

const char *gc_scenario_label(void) { return "AR_HW/generic"; }
const char *gc_scenario_out_path(void) { return "../actual/ar_hw_generic_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    (void)ram;

    // Test ARSetSize (empty stub).
    ARSetSize();

    // Test ARRegisterDMACallback.
    ARCallback old1 = ARRegisterDMACallback(dummy_callback);
    ARCallback old2 = ARRegisterDMACallback((ARCallback)0);

    // Test ARGetDMAStatus before any DMA.
    u32 status_before = ARGetDMAStatus();

    // Test ARStartDMA: MRAM->ARAM.
    ARStartDMA(0, 0x80100000u, 0x00004000u, 0x00001000u);

    u32 status_after = ARGetDMAStatus();

    // Test ARStartDMA again: ARAM->MRAM.
    ARStartDMA(1, 0x80200000u, 0x00008000u, 0x00002000u);

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x40);
    if (!p) die("gc_ram_ptr failed");
    for (u32 i = 0; i < 0x40; i += 4) wr32be(p + i, 0);

    u32 off = 0;
    wr32be(p + off, 0xDEADBEEFu); off += 4;
    wr32be(p + off, (u32)(old1 == 0 ? 0 : 1)); off += 4;
    wr32be(p + off, (u32)(old2 != 0 ? 1 : 0)); off += 4;
    wr32be(p + off, status_before); off += 4;
    wr32be(p + off, status_after); off += 4;
    wr32be(p + off, gc_ar_dma_type); off += 4;
    wr32be(p + off, gc_ar_dma_mainmem); off += 4;
    wr32be(p + off, gc_ar_dma_aram); off += 4;
    wr32be(p + off, gc_ar_dma_length); off += 4;
}
