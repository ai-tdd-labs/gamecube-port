#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

extern uint32_t gc_gx_tevc[16];
extern uint32_t gc_gx_bp_sent_not;

typedef uint32_t u32;
typedef uint8_t u8;

typedef struct { uint8_t _dummy; } GXFifoObj;
GXFifoObj *GXInit(void *base, u32 size);
void GXSetTevColorOp(u32 stage, u32 op, u32 bias, u32 scale, u32 clamp, u32 out_reg);

enum { GX_TEVSTAGE0 = 0 };
enum { GX_TEV_ADD = 0 };
enum { GX_TB_ZERO = 0 };
enum { GX_CS_SCALE_1 = 0 };
enum { GX_TRUE = 1 };
enum { GX_TEVPREV = 0 };

const char *gc_scenario_label(void) { return "GXSetTevColorOp/mp4_init_gx"; }
const char *gc_scenario_out_path(void) { return "../actual/gx_set_tev_color_op_mp4_init_gx_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    static u8 fifo[0x20000];
    GXInit(fifo, sizeof(fifo));

    GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x0C);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0x00, 0xDEADBEEFu);
    wr32be(p + 0x04, gc_gx_tevc[0]);
    wr32be(p + 0x08, gc_gx_bp_sent_not);
}
