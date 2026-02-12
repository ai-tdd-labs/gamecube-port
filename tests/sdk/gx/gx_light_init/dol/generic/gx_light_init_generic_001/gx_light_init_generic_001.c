#include <stdint.h>
typedef uint32_t u32;
typedef uint8_t  u8;

typedef struct {
    u32 reserved[3];
    u32 Color;
    float a[3];
    float k[3];
    float lpos[3];
    float ldir[3];
} GXLightObj;

typedef struct { u8 r, g, b, a; } GXColor;

void GXInitLightAttn(GXLightObj *lt_obj, float a0, float a1, float a2,
                     float k0, float k1, float k2);
void GXInitLightAttnK(GXLightObj *lt_obj, float k0, float k1, float k2);
void GXInitLightPos(GXLightObj *lt_obj, float x, float y, float z);
void GXInitLightDir(GXLightObj *lt_obj, float nx, float ny, float nz);
void GXInitLightColor(GXLightObj *lt_obj, GXColor color);

/* gc_mem init */
#include "src/sdk_port/gc_mem.h"
#include "src/sdk_port/sdk_state.h"

static inline void wr32be(volatile uint8_t *p, u32 v) {
    p[0]=(uint8_t)(v>>24); p[1]=(uint8_t)(v>>16);
    p[2]=(uint8_t)(v>>8);  p[3]=(uint8_t)(v);
}
static inline u32 fbits(float f) { u32 u; __builtin_memcpy(&u, &f, 4); return u; }

/* Dump full 64-byte light object (16 u32s) */
static inline void dump_light(volatile uint8_t *p, const GXLightObj *obj) {
    const u32 *raw = (const u32 *)obj;
    for (int i = 0; i < 16; i++) {
        wr32be(p + i * 4, raw[i]);
    }
}

int main(void) {
    gc_mem_set(0x80000000u, 0x01800000u, (uint8_t *)0x80000000u);
    gc_sdk_state_reset();

    volatile uint8_t *out = (volatile uint8_t *)0x80300000;
    for (u32 i = 0; i < 0x180; i++) out[i] = 0;

    u32 off = 0;
    GXLightObj obj;
    __builtin_memset(&obj, 0, sizeof(obj));

    /* --- T1: GXInitLightAttn --- */
    GXInitLightAttn(&obj, 1.0f, 0.5f, 0.25f, 2.0f, 1.5f, 0.75f);
    wr32be(out + off, 0x1160A001u); off += 4;
    dump_light(out + off, &obj); off += 64;

    /* --- T2: GXInitLightAttnK --- */
    __builtin_memset(&obj, 0, sizeof(obj));
    GXInitLightAttnK(&obj, 3.0f, 0.1f, 0.01f);
    wr32be(out + off, 0x1160A002u); off += 4;
    dump_light(out + off, &obj); off += 64;

    /* --- T3: GXInitLightPos --- */
    __builtin_memset(&obj, 0, sizeof(obj));
    GXInitLightPos(&obj, 100.0f, -200.0f, 50.5f);
    wr32be(out + off, 0x1160A003u); off += 4;
    dump_light(out + off, &obj); off += 64;

    /* --- T4: GXInitLightDir (note: stores negated values) --- */
    __builtin_memset(&obj, 0, sizeof(obj));
    GXInitLightDir(&obj, 0.0f, 1.0f, 0.0f);
    wr32be(out + off, 0x1160A004u); off += 4;
    dump_light(out + off, &obj); off += 64;

    /* --- T5: GXInitLightColor --- */
    __builtin_memset(&obj, 0, sizeof(obj));
    {
        GXColor c = {0xFF, 0x80, 0x40, 0x20};
        GXInitLightColor(&obj, c);
    }
    wr32be(out + off, 0x1160A005u); off += 4;
    dump_light(out + off, &obj); off += 64;

    /* --- T6: Combined: all 5 functions on same object --- */
    __builtin_memset(&obj, 0, sizeof(obj));
    GXInitLightPos(&obj, 10.0f, 20.0f, 30.0f);
    GXInitLightDir(&obj, 0.577f, 0.577f, 0.577f);
    GXInitLightAttn(&obj, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f);
    {
        GXColor c = {0xAA, 0xBB, 0xCC, 0xDD};
        GXInitLightColor(&obj, c);
    }
    wr32be(out + off, 0x1160A006u); off += 4;
    dump_light(out + off, &obj); off += 64;
    /* off = 6*(4+64) = 408 = 0x198 -- need 0x1A0 dump */

    while (1) { __asm__ volatile("nop"); }
    return 0;
}
