#include <stdint.h>
#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

typedef uint32_t u32;

void GXSetTevSwapMode(u32 stage, u32 ras_sel, u32 tex_sel);
void GXSetTevSwapModeTable(u32 table, u32 red, u32 green, u32 blue, u32 alpha);

extern u32 gc_gx_teva[16];
extern u32 gc_gx_tev_ksel[8];
extern u32 gc_gx_bp_sent_not;

const char *gc_scenario_label(void) { return "GXSetTevSwapMode/generic"; }
const char *gc_scenario_out_path(void) { return "../actual/gx_set_tev_swap_mode_generic_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    (void)ram;

    GXSetTevSwapMode(0, 1, 2);
    GXSetTevSwapMode(5, 3, 0);
    GXSetTevSwapModeTable(0, 0, 1, 2, 0);
    GXSetTevSwapModeTable(2, 3, 2, 1, 0);

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x40);
    if (!p) die("gc_ram_ptr failed");
    for (u32 i = 0; i < 0x40; i += 4) wr32be(p + i, 0);

    u32 off = 0;
    wr32be(p + off, 0xDEADBEEFu); off += 4;
    wr32be(p + off, gc_gx_teva[0]); off += 4;
    wr32be(p + off, gc_gx_teva[5]); off += 4;
    wr32be(p + off, gc_gx_tev_ksel[0]); off += 4;
    wr32be(p + off, gc_gx_tev_ksel[1]); off += 4;
    wr32be(p + off, gc_gx_tev_ksel[4]); off += 4;
    wr32be(p + off, gc_gx_tev_ksel[5]); off += 4;
    wr32be(p + off, gc_gx_bp_sent_not); off += 4;
}
