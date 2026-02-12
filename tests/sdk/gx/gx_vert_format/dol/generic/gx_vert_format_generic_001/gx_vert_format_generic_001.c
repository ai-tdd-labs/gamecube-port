#include <stdint.h>
#include "src/sdk_port/gc_mem.h"
#include "src/sdk_port/sdk_state.h"

typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t  u8;
typedef int16_t  s16;

void GXColor1x16(u16 index);
void GXColor4u8(u8 r, u8 g, u8 b, u8 a);
void GXNormal1x16(u16 index);
void GXNormal3s16(s16 x, s16 y, s16 z);
void GXTexCoord1x16(u16 index);
void GXTexCoord2s16(s16 s, s16 t);

extern u32 gc_gx_color1x16_last;
extern u32 gc_gx_color4u8_last;
extern u32 gc_gx_normal1x16_last;
extern u32 gc_gx_normal3s16_x;
extern u32 gc_gx_normal3s16_y;
extern u32 gc_gx_normal3s16_z;
extern u32 gc_gx_texcoord1x16_last;
extern u32 gc_gx_texcoord2s16_s;
extern u32 gc_gx_texcoord2s16_t;

static inline void wr32be(volatile uint8_t *p, u32 v) {
    p[0]=(uint8_t)(v>>24); p[1]=(uint8_t)(v>>16);
    p[2]=(uint8_t)(v>>8);  p[3]=(uint8_t)(v);
}

int main(void) {
    gc_mem_set(0x80000000u, 0x01800000u, (uint8_t *)0x80000000u);
    gc_sdk_state_reset();

    volatile uint8_t *out = (volatile uint8_t *)0x80300000;
    for (u32 i = 0; i < 0x40; i++) out[i] = 0;

    GXColor1x16(0x1234);
    GXColor4u8(0xAA, 0xBB, 0xCC, 0xDD);
    GXNormal1x16(0x5678);
    GXNormal3s16(-100, 200, -300);
    GXTexCoord1x16(0xABCD);
    GXTexCoord2s16(-500, 1000);

    u32 off = 0;
    wr32be(out + off, 0xDEADBEEFu); off += 4;
    wr32be(out + off, gc_gx_color1x16_last); off += 4;
    wr32be(out + off, gc_gx_color4u8_last); off += 4;
    wr32be(out + off, gc_gx_normal1x16_last); off += 4;
    wr32be(out + off, gc_gx_normal3s16_x); off += 4;
    wr32be(out + off, gc_gx_normal3s16_y); off += 4;
    wr32be(out + off, gc_gx_normal3s16_z); off += 4;
    wr32be(out + off, gc_gx_texcoord1x16_last); off += 4;
    wr32be(out + off, gc_gx_texcoord2s16_s); off += 4;
    wr32be(out + off, gc_gx_texcoord2s16_t); off += 4;
    // Total: 10 * 4 = 0x28

    while (1) { __asm__ volatile("nop"); }
    return 0;
}
