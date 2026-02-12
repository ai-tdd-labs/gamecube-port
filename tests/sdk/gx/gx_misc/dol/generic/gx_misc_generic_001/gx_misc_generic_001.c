#include <stdint.h>
#include "src/sdk_port/gc_mem.h"
#include "src/sdk_port/sdk_state.h"

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

static inline void wr32be(volatile uint8_t *p, u32 v) {
    p[0]=(uint8_t)(v>>24); p[1]=(uint8_t)(v>>16);
    p[2]=(uint8_t)(v>>8);  p[3]=(uint8_t)(v);
}

static inline u32 f32_to_u32(f32 f) {
    u32 r;
    __builtin_memcpy(&r, &f, sizeof(u32));
    return r;
}

int main(void) {
    gc_mem_set(0x80000000u, 0x01800000u, (uint8_t *)0x80000000u);
    gc_sdk_state_reset();

    volatile uint8_t *out = (volatile uint8_t *)0x80300000;
    for (u32 i = 0; i < 0x40; i++) out[i] = 0;

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

    u32 off = 0;
    wr32be(out + off, 0xDEADBEEFu); off += 4;

    // GXGetProjectionv results: 7 floats as u32 bits.
    for (u32 i = 0; i < 7; i++) {
        wr32be(out + off, f32_to_u32(pv[i])); off += 4;
    }

    // GXPixModeSync results.
    wr32be(out + off, gc_gx_last_ras_reg); off += 4;
    wr32be(out + off, gc_gx_bp_sent_not); off += 4;
    // Total: 10 * 4 = 0x28

    while (1) { __asm__ volatile("nop"); }
    return 0;
}
