#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

typedef uint32_t u32;
typedef uint16_t u16;

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

void GXAdjustForOverscan(GXRenderModeObj *rmin, GXRenderModeObj *rmout, u16 hor, u16 ver);

const char *gc_scenario_label(void) { return "GXAdjustForOverscan/mp4_hu_sys_init"; }
const char *gc_scenario_out_path(void) { return "../actual/gx_adjust_for_overscan_mp4_hu_sys_init_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    GXRenderModeObj in = {
        .viTVmode = 0,
        .fbWidth = 640,
        .efbHeight = 480,
        .xfbHeight = 480,
        .viXOrigin = 40,
        .viYOrigin = 0,
        .viWidth = 640,
        .viHeight = 480,
        .xFBmode = 1,
    };
    GXRenderModeObj out;

    GXAdjustForOverscan(&in, &out, 0, 16);

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x20);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0x00, 0xDEADBEEFu);
    wr32be(p + 0x04, out.fbWidth);
    wr32be(p + 0x08, out.efbHeight);
    wr32be(p + 0x0C, out.xfbHeight);
    wr32be(p + 0x10, out.viXOrigin);
    wr32be(p + 0x14, out.viYOrigin);
    wr32be(p + 0x18, out.viWidth);
    wr32be(p + 0x1C, out.viHeight);
}
