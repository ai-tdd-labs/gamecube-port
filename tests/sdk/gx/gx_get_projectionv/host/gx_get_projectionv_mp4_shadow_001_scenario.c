#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

typedef float f32;
typedef uint32_t u32;

typedef enum {
    GX_PERSPECTIVE = 0,
    GX_ORTHOGRAPHIC = 1,
} GXProjectionType;

void GXSetProjection(f32 mtx[4][4], GXProjectionType type);
void GXGetProjectionv(f32 *ptr);

extern u32 gc_gx_bp_sent_not;
extern u32 gc_gx_proj_type;
extern u32 gc_gx_proj_mtx_bits[6];

const char *gc_scenario_label(void) { return "gx_get_projectionv/mp4_shadow"; }
const char *gc_scenario_out_path(void) {
    return "../actual/gx_get_projectionv_mp4_shadow_001.bin";
}

void gc_scenario_run(GcRam *ram) {
    f32 mtx[4][4] = {
        {2.0f, 0.0f, 0.0f, 5.0f},
        {0.0f, 3.0f, 0.0f, 7.0f},
        {0.0f, 0.0f, 11.0f, 13.0f},
        {0.0f, 0.0f, 0.0f, 1.0f},
    };
    f32 out[7];
    u32 *out_bits = (u32 *)out;

    gc_gx_bp_sent_not = 0;
    gc_gx_proj_type = 0;
    for (u32 i = 0; i < 6; i++) {
        gc_gx_proj_mtx_bits[i] = 0;
    }

    GXSetProjection(mtx, GX_ORTHOGRAPHIC);
    GXGetProjectionv(out);

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x24);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0x00, 0xDEADBEEFu);
    wr32be(p + 0x04, out_bits[0]);
    wr32be(p + 0x08, out_bits[1]);
    wr32be(p + 0x0C, out_bits[2]);
    wr32be(p + 0x10, out_bits[3]);
    wr32be(p + 0x14, out_bits[4]);
    wr32be(p + 0x18, out_bits[5]);
    wr32be(p + 0x1C, out_bits[6]);
    wr32be(p + 0x20, gc_gx_bp_sent_not);
}
