#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

typedef uint32_t u32;
typedef uint16_t u16;

u32 GXGetTexBufferSize(u16 width, u16 height, u32 format, uint8_t mipmap, uint8_t max_lod);

const char *gc_scenario_label(void) { return "GXGetTexBufferSize/mp4_wipe_cross_fade"; }
const char *gc_scenario_out_path(void) { return "../actual/gx_get_tex_buffer_size_mp4_wipe_cross_fade_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    const u16 w = 640;
    const u16 h = 480;
    const u32 fmt = 0x4; // GX_TF_RGB565
    const uint8_t mipmap = 0;
    const uint8_t max_lod = 0;

    u32 size = GXGetTexBufferSize(w, h, fmt, mipmap, max_lod);

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x40);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0x00, 0xDEADBEEFu);
    wr32be(p + 0x04, size);
    for (u32 off = 0x08; off < 0x40; off += 4) {
        wr32be(p + off, 0);
    }
}
