#include <stdint.h>
#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

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

const char *gc_scenario_label(void) { return "GXVertFormat/generic"; }
const char *gc_scenario_out_path(void) { return "../actual/gx_vert_format_generic_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    (void)ram;

    GXColor1x16(0x1234);
    GXColor4u8(0xAA, 0xBB, 0xCC, 0xDD);
    GXNormal1x16(0x5678);
    GXNormal3s16(-100, 200, -300);
    GXTexCoord1x16(0xABCD);
    GXTexCoord2s16(-500, 1000);

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x40);
    if (!p) die("gc_ram_ptr failed");
    for (u32 i = 0; i < 0x40; i += 4) wr32be(p + i, 0);

    u32 off = 0;
    wr32be(p + off, 0xDEADBEEFu); off += 4;
    wr32be(p + off, gc_gx_color1x16_last); off += 4;
    wr32be(p + off, gc_gx_color4u8_last); off += 4;
    wr32be(p + off, gc_gx_normal1x16_last); off += 4;
    wr32be(p + off, gc_gx_normal3s16_x); off += 4;
    wr32be(p + off, gc_gx_normal3s16_y); off += 4;
    wr32be(p + off, gc_gx_normal3s16_z); off += 4;
    wr32be(p + off, gc_gx_texcoord1x16_last); off += 4;
    wr32be(p + off, gc_gx_texcoord2s16_s); off += 4;
    wr32be(p + off, gc_gx_texcoord2s16_t); off += 4;
}
