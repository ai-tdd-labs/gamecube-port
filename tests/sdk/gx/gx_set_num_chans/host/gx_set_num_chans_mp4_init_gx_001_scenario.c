#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

typedef uint32_t u32;
typedef uint8_t u8;

typedef struct { uint8_t _dummy; } GXFifoObj;
GXFifoObj *GXInit(void *base, u32 size);
void GXSetNumChans(u8 nChans);

extern u32 gc_gx_gen_mode;
extern u32 gc_gx_dirty_state;

const char *gc_scenario_label(void) { return "GXSetNumChans/mp4_init_gx"; }
const char *gc_scenario_out_path(void) { return "../actual/gx_set_num_chans_mp4_init_gx_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    static u8 fifo[0x20000];
    GXInit(fifo, sizeof(fifo));

    GXSetNumChans(0);

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x10);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0x00, 0xDEADBEEFu);
    wr32be(p + 0x04, gc_gx_gen_mode);
    wr32be(p + 0x08, gc_gx_dirty_state);
}
