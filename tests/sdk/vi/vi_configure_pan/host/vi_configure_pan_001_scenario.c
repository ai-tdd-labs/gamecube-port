#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

void VIConfigurePan(uint16_t xOrg, uint16_t yOrg, uint16_t width, uint16_t height);

extern uint32_t gc_vi_pan_pos_x;
extern uint32_t gc_vi_pan_pos_y;
extern uint32_t gc_vi_pan_size_x;
extern uint32_t gc_vi_pan_size_y;
extern uint32_t gc_vi_disp_size_y;
extern uint32_t gc_vi_helper_calls;
extern uint32_t gc_vi_non_inter;
extern uint32_t gc_vi_xfb_mode;

const char *gc_scenario_label(void) { return "VIConfigurePan/legacy_001"; }
const char *gc_scenario_out_path(void) { return "../actual/vi_configure_pan_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    enum { VI_NON_INTERLACE = 1, VI_XFBMODE_SF = 1 };

    gc_vi_pan_pos_x = 0;
    gc_vi_pan_pos_y = 0;
    gc_vi_pan_size_x = 0;
    gc_vi_pan_size_y = 0;
    gc_vi_disp_size_y = 0;
    gc_vi_non_inter = VI_NON_INTERLACE;
    gc_vi_xfb_mode = VI_XFBMODE_SF;
    gc_vi_helper_calls = 0;

    // Mirror the legacy DOL testcase inputs.
    VIConfigurePan(10, 20, 320, 240);

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 28);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0x00, 0xDEADBEEFu);
    wr32be(p + 0x04, gc_vi_pan_pos_x);
    wr32be(p + 0x08, gc_vi_pan_pos_y);
    wr32be(p + 0x0C, gc_vi_pan_size_x);
    wr32be(p + 0x10, gc_vi_pan_size_y);
    wr32be(p + 0x14, gc_vi_disp_size_y);
    wr32be(p + 0x18, gc_vi_helper_calls);
}
