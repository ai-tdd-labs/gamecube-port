#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

typedef struct { uint8_t _dummy; } GXFifoObj;
GXFifoObj *GXInit(void *base, uint32_t size);

extern struct {
    uint32_t inDispList;
    uint32_t dlSaveContext;
    uint32_t genMode;
    uint32_t bpMask;
} gc_gx_data;

const char *gc_scenario_label(void) { return "GXInit/mp4_hu_sys_init"; }
const char *gc_scenario_out_path(void) { return "../actual/gx_init_mp4_default_fifo_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    // MP4 HuSysInit callsite shape:
    //   DefaultFifo = OSAlloc(0x100000) -> typically returns (cell + 0x20)
    //   GXInit(DefaultFifo, 0x100000)
    (void)GXInit((void *)0x80003020u, 0x100000u);

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x18);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0x00, 0xDEADBEEFu);
    wr32be(p + 0x04, 0x80003020u);
    wr32be(p + 0x08, gc_gx_data.inDispList);
    wr32be(p + 0x0C, gc_gx_data.dlSaveContext);
    wr32be(p + 0x10, gc_gx_data.genMode);
    wr32be(p + 0x14, gc_gx_data.bpMask);
}
