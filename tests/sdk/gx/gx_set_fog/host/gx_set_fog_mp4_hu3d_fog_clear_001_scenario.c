#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

typedef uint32_t u32;
typedef uint8_t u8;
typedef float f32;

typedef struct { u8 r, g, b, a; } GXColor;

typedef enum {
    GX_FOG_NONE = 0,
} GXFogType;

void GXSetFog(GXFogType type, f32 startz, f32 endz, f32 nearz, f32 farz, GXColor color);

extern u32 gc_gx_fog0;
extern u32 gc_gx_fog1;
extern u32 gc_gx_fog2;
extern u32 gc_gx_fog3;
extern u32 gc_gx_fogclr;
extern u32 gc_gx_last_ras_reg;
extern u32 gc_gx_bp_sent_not;

const char *gc_scenario_label(void) { return "GXSetFog/mp4_hu3d_fog_clear"; }
const char *gc_scenario_out_path(void) { return "../actual/gx_set_fog_mp4_hu3d_fog_clear_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    GXColor bg = {0, 0, 0, 0xFF};

    gc_gx_fog0 = gc_gx_fog1 = gc_gx_fog2 = gc_gx_fog3 = gc_gx_fogclr = 0;
    gc_gx_last_ras_reg = 0;
    gc_gx_bp_sent_not = 1;

    GXSetFog(GX_FOG_NONE, 0.0f, 0.0f, 0.0f, 0.0f, bg);

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x20);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0x00, 0xDEADBEEFu);
    wr32be(p + 0x04, gc_gx_fog0);
    wr32be(p + 0x08, gc_gx_fog1);
    wr32be(p + 0x0C, gc_gx_fog2);
    wr32be(p + 0x10, gc_gx_fog3);
    wr32be(p + 0x14, gc_gx_fogclr);
    wr32be(p + 0x18, gc_gx_last_ras_reg);
    wr32be(p + 0x1C, gc_gx_bp_sent_not);
}
