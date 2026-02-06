#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

extern uint32_t gc_gx_tref[8];
extern uint32_t gc_gx_texmap_id[16];
extern uint32_t gc_gx_tev_tc_enab;
extern uint32_t gc_gx_dirty_state;
extern uint32_t gc_gx_bp_sent_not;

typedef uint32_t u32;
typedef uint8_t u8;

typedef struct { uint8_t _dummy; } GXFifoObj;
GXFifoObj *GXInit(void *base, u32 size);
void GXSetTevOrder(u32 stage, u32 coord, u32 map, u32 color);

enum { GX_TEVSTAGE0 = 0 };
enum { GX_TEXCOORD0 = 0 };
enum { GX_TEXMAP0 = 0 };
enum { GX_COLOR0A0 = 4 };

const char *gc_scenario_label(void) { return "GXSetTevOrder/mp4_init_gx"; }
const char *gc_scenario_out_path(void) { return "../actual/gx_set_tev_order_mp4_init_gx_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    static u8 fifo[0x20000];
    GXInit(fifo, sizeof(fifo));

    GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x18);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0x00, 0xDEADBEEFu);
    wr32be(p + 0x04, gc_gx_tref[0]);
    wr32be(p + 0x08, gc_gx_texmap_id[0]);
    wr32be(p + 0x0C, gc_gx_tev_tc_enab);
    wr32be(p + 0x10, gc_gx_dirty_state);
    wr32be(p + 0x14, gc_gx_bp_sent_not);
}
