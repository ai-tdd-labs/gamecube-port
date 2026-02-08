#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;

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

typedef struct {
    uint8_t _dummy;
} GXFifoObj;

GXFifoObj *GXInit(void *base, u32 size);
void GXInitTexObj(GXTexObj *obj, void *image_ptr, u16 width, u16 height, u32 format, u32 wrap_s, u32 wrap_t, u8 mipmap);
void GXLoadTexObj(GXTexObj *obj, u32 id);

// Observable "writes" from sdk_port (mirrors what GXLoadTexObjPreLoaded sends as BP regs).
extern u32 gc_gx_tex_load_mode0_last;
extern u32 gc_gx_tex_load_mode1_last;
extern u32 gc_gx_tex_load_image0_last;
extern u32 gc_gx_tex_load_image1_last;
extern u32 gc_gx_tex_load_image2_last;
extern u32 gc_gx_tex_load_image3_last;

const char *gc_scenario_label(void) { return "GXLoadTexObj/mp4_wipe_texmap0"; }
const char *gc_scenario_out_path(void) { return "../actual/gx_load_tex_obj_mp4_wipe_texmap0_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    enum {
        GX_TF_RGB565 = 0x4,
        GX_CLAMP = 0,
        GX_TEXMAP0 = 0,
    };

    GXInit((void *)0, 0);

    GXTexObj tex;
    void *img = (void *)(uintptr_t)0x80010000u;
    GXInitTexObj(&tex, img, 640, 480, GX_TF_RGB565, GX_CLAMP, GX_CLAMP, 0);
    GXLoadTexObj(&tex, GX_TEXMAP0);

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x40);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0x00, 0xDEADBEEFu);
    wr32be(p + 0x04, gc_gx_tex_load_mode0_last);
    wr32be(p + 0x08, gc_gx_tex_load_mode1_last);
    wr32be(p + 0x0C, gc_gx_tex_load_image0_last);
    wr32be(p + 0x10, gc_gx_tex_load_image1_last);
    wr32be(p + 0x14, gc_gx_tex_load_image2_last);
    wr32be(p + 0x18, gc_gx_tex_load_image3_last);
    for (u32 off = 0x1C; off < 0x40; off += 4) {
        wr32be(p + off, 0);
    }
}

