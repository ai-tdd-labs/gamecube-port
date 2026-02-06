#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

extern uint32_t gc_gx_vcd_lo;
extern uint32_t gc_gx_vcd_hi;
extern uint32_t gc_gx_has_nrms;
extern uint32_t gc_gx_has_binrms;
extern uint32_t gc_gx_nrm_type;
extern uint32_t gc_gx_dirty_state;

typedef uint32_t u32;
typedef uint8_t u8;

typedef struct { uint8_t _dummy; } GXFifoObj;
GXFifoObj *GXInit(void *base, u32 size);
void GXSetVtxDesc(u32 attr, u32 type);

enum { GX_VA_POS = 9 };
enum { GX_DIRECT = 1 };

const char *gc_scenario_label(void) { return "GXSetVtxDesc/mp4_init_gx"; }
const char *gc_scenario_out_path(void) { return "../actual/gx_set_vtx_desc_mp4_init_gx_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    static u8 fifo[0x20000];
    GXInit(fifo, sizeof(fifo));

    GXSetVtxDesc(GX_VA_POS, GX_DIRECT);

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x1C);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0x00, 0xDEADBEEFu);
    wr32be(p + 0x04, gc_gx_vcd_lo);
    wr32be(p + 0x08, gc_gx_vcd_hi);
    wr32be(p + 0x0C, gc_gx_has_nrms);
    wr32be(p + 0x10, gc_gx_has_binrms);
    wr32be(p + 0x14, gc_gx_nrm_type);
    wr32be(p + 0x18, gc_gx_dirty_state);
}
