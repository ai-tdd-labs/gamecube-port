#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

typedef uint32_t u32;
typedef uint8_t u8;

typedef struct { u8 r, g, b, a; } GXColor;

void GXSetCopyClear(GXColor clear_clr, u32 clear_z);

extern u32 gc_gx_copy_clear_reg0;
extern u32 gc_gx_copy_clear_reg1;
extern u32 gc_gx_copy_clear_reg2;
extern u32 gc_gx_last_ras_reg;
extern u32 gc_gx_bp_sent_not;

const char *gc_scenario_label(void) { return "GXSetCopyClear/mp4_hu3d_preproc"; }
const char *gc_scenario_out_path(void) { return "../actual/gx_set_copy_clear_mp4_hu3d_preproc_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    GXColor bg = {0, 0, 0, 0xFF};
    gc_gx_last_ras_reg = 0;
    gc_gx_bp_sent_not = 1;
    gc_gx_copy_clear_reg0 = 0;
    gc_gx_copy_clear_reg1 = 0;
    gc_gx_copy_clear_reg2 = 0;

    GXSetCopyClear(bg, 0x00FFFFFFu);

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x18);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0x00, 0xDEADBEEFu);
    wr32be(p + 0x04, gc_gx_copy_clear_reg0);
    wr32be(p + 0x08, gc_gx_copy_clear_reg1);
    wr32be(p + 0x0C, gc_gx_copy_clear_reg2);
    wr32be(p + 0x10, gc_gx_last_ras_reg);
    wr32be(p + 0x14, gc_gx_bp_sent_not);
}
