#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

typedef uint16_t u16;
typedef uint32_t u32;

typedef struct {
    u32 viTVmode;
    u16 fbWidth;
    u16 efbHeight;
    u16 xfbHeight;
    u16 viXOrigin;
    u16 viYOrigin;
    u16 viWidth;
    u16 viHeight;
    u32 xFBmode;
} GXRenderModeObj;

void VIConfigure(const void *obj);

extern uint32_t gc_vi_change_mode;
extern uint32_t gc_vi_disp_pos_x;
extern uint32_t gc_vi_disp_pos_y;
extern uint32_t gc_vi_disp_size_x;
extern uint32_t gc_vi_fb_size_x;
extern uint32_t gc_vi_fb_size_y;
extern uint32_t gc_vi_xfb_mode;
extern uint32_t gc_vi_helper_calls;
extern uint32_t gc_vi_non_inter;

const char *gc_scenario_label(void) { return "VIConfigure/mp4_hu_sys_init"; }
const char *gc_scenario_out_path(void) { return "../actual/vi_configure_mp4_hu_sys_init_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    // Mirror the legacy DOL testcase inputs so expected.bin is a meaningful oracle.
    enum { VI_NTSC = 0, VI_NON_INTERLACE = 1, VI_XFBMODE_SF = 1 };

    GXRenderModeObj obj;
    gc_vi_change_mode = 0;
    gc_vi_non_inter = 0;
    gc_vi_helper_calls = 0;

    obj.viTVmode = 0;
    
    obj.viXOrigin = 40;
    
    obj.viYOrigin = 0;
    
    obj.viWidth = 640;
    
    obj.fbWidth = 640;
    
    obj.xfbHeight = 480;
    
    obj.xFBmode = 1;
    

    VIConfigure(&obj);

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x24);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0x00, 0xDEADBEEFu);
    wr32be(p + 0x04, gc_vi_change_mode);
    wr32be(p + 0x08, gc_vi_disp_pos_x);
    wr32be(p + 0x0C, gc_vi_disp_pos_y);
    wr32be(p + 0x10, gc_vi_disp_size_x);
    wr32be(p + 0x14, gc_vi_fb_size_x);
    wr32be(p + 0x18, gc_vi_fb_size_y);
    wr32be(p + 0x1C, gc_vi_xfb_mode);
    wr32be(p + 0x20, gc_vi_helper_calls);
}
