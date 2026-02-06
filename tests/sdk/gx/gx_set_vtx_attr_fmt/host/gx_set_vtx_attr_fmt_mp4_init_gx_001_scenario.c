#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

extern uint32_t gc_gx_vat_a[8];
extern uint32_t gc_gx_vat_b[8];
extern uint32_t gc_gx_vat_c[8];
extern uint32_t gc_gx_dirty_state;
extern uint32_t gc_gx_dirty_vat;

typedef uint32_t u32;
typedef uint8_t u8;

typedef struct { uint8_t _dummy; } GXFifoObj;
GXFifoObj *GXInit(void *base, u32 size);
void GXSetVtxAttrFmt(u32 vtxfmt, u32 attr, u32 cnt, u32 type, u8 frac);

enum { GX_VA_POS = 9 };
enum { GX_POS_XYZ = 1 };
enum { GX_F32 = 4 };

const char *gc_scenario_label(void) { return "GXSetVtxAttrFmt/mp4_init_gx"; }
const char *gc_scenario_out_path(void) { return "../actual/gx_set_vtx_attr_fmt_mp4_init_gx_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    static u8 fifo[0x20000];
    GXInit(fifo, sizeof(fifo));

    GXSetVtxAttrFmt(0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x18);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0x00, 0xDEADBEEFu);
    wr32be(p + 0x04, gc_gx_vat_a[0]);
    wr32be(p + 0x08, gc_gx_vat_b[0]);
    wr32be(p + 0x0C, gc_gx_vat_c[0]);
    wr32be(p + 0x10, gc_gx_dirty_state);
    wr32be(p + 0x14, gc_gx_dirty_vat);
}
