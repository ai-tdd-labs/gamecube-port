#include <stdint.h>
#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t  u8;
typedef float    f32;

typedef enum { GX_PERSPECTIVE = 0, GX_ORTHOGRAPHIC = 1 } GXProjectionType;

void GXSetProjection(f32 mtx[4][4], GXProjectionType type);
void GXGetProjectionv(f32 *ptr);
void GXPixModeSync(void);

extern u32 gc_gx_pe_ctrl;
extern u32 gc_gx_bp_sent_not;
extern u32 gc_gx_last_ras_reg;

static inline u32 f32_to_u32(f32 f) {
    u32 r;
    __builtin_memcpy(&r, &f, sizeof(u32));
    return r;
}

const char *gc_scenario_label(void) { return "GXMisc/generic"; }
const char *gc_scenario_out_path(void) { return "../actual/gx_misc_generic_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    (void)ram;

    // Set up a perspective projection with known values.
    f32 mtx[4][4] = {
        { 1.5f, 0.0f, 0.0f, 0.0f },
        { 0.0f, 2.0f, 0.0f, 0.0f },
        { 0.1f, 0.2f, -1.001f, -0.1001f },
        { 0.0f, 0.0f, -1.0f, 0.0f }
    };
    GXSetProjection(mtx, GX_PERSPECTIVE);

    // Retrieve projection via GXGetProjectionv.
    f32 pv[7];
    for (u32 i = 0; i < 7; i++) pv[i] = 0.0f;
    GXGetProjectionv(pv);

    // Set pe_ctrl to a known value, then call GXPixModeSync.
    gc_gx_pe_ctrl = 0x47000042u;
    GXPixModeSync();

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x40);
    if (!p) die("gc_ram_ptr failed");
    for (u32 i = 0; i < 0x40; i += 4) wr32be(p + i, 0);

    u32 off = 0;
    wr32be(p + off, 0xDEADBEEFu); off += 4;

    // GXGetProjectionv results: 7 floats as u32 bits.
    for (u32 i = 0; i < 7; i++) {
        wr32be(p + off, f32_to_u32(pv[i])); off += 4;
    }

    // GXPixModeSync results.
    wr32be(p + off, gc_gx_last_ras_reg); off += 4;
    wr32be(p + off, gc_gx_bp_sent_not); off += 4;
}
