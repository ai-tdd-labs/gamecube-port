#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;
typedef float f32;

typedef struct {
    u32 mode0;
    u32 mode1;
    u32 image0;
    u32 image3;
    u32 userData;
    u32 fmt;
    u32 tlutName;
    u16 loadCnt;
    u8 loadFmt;
    u8 flags;
} GXTexObj;

void GXInitTexObj(GXTexObj *obj, void *image_ptr, u16 width, u16 height, u32 format, u32 wrap_s, u32 wrap_t, u8 mipmap);
void GXInitTexObjLOD(GXTexObj *obj, u32 min_filt, u32 mag_filt, f32 min_lod, f32 max_lod, f32 lod_bias, u8 bias_clamp, u8 do_edge_lod, u32 max_aniso);

const char *gc_scenario_label(void) { return "GXInitTexObjLOD/mp4_pfdrawfonts"; }
const char *gc_scenario_out_path(void) { return "../actual/gx_init_tex_obj_lod_mp4_pfdrawfonts_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    enum { GX_NEAR = 0, GX_ANISO_1 = 0 };

    GXTexObj tex;
    GXInitTexObj(&tex, (void *)(uintptr_t)0x80010000u, 128, 128, 0, 0, 0, 0);

    GXInitTexObjLOD(&tex, GX_NEAR, GX_NEAR, 0.0f, 0.0f, 0.0f, 0, 0, GX_ANISO_1);

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x20);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0x00, 0xDEADBEEFu);
    wr32be(p + 0x04, tex.mode0);
    wr32be(p + 0x08, tex.mode1);
    for (u32 off = 0x0C; off < 0x20; off += 4) {
        wr32be(p + off, 0);
    }
}
