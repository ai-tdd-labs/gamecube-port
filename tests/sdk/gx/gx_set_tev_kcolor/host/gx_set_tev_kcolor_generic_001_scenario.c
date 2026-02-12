#include <stdint.h>
#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t  u8;
typedef int16_t  s16;

typedef struct { u8 r, g, b, a; } GXColor;
typedef struct { s16 r, g, b, a; } GXColorS10;

void GXSetTevKColor(u32 id, GXColor color);
void GXSetTevKColorSel(u32 stage, u32 sel);
void GXSetTevKAlphaSel(u32 stage, u32 sel);
void GXSetTevColorS10(u32 id, GXColorS10 color);

extern u32 gc_gx_tev_ksel[8];
extern u32 gc_gx_tev_kcolor_ra[4];
extern u32 gc_gx_tev_kcolor_bg[4];
extern u32 gc_gx_tev_colors10_ra_last;
extern u32 gc_gx_tev_colors10_bg_last;
extern u32 gc_gx_bp_sent_not;

const char *gc_scenario_label(void) { return "GXSetTevKColor/generic"; }
const char *gc_scenario_out_path(void) { return "../actual/gx_set_tev_kcolor_generic_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    (void)ram;

    GXSetTevKColor(0, (GXColor){0x11, 0x22, 0x33, 0x44});
    GXSetTevKColor(2, (GXColor){0xAA, 0xBB, 0xCC, 0xDD});
    GXSetTevKColorSel(0, 12);
    GXSetTevKColorSel(3, 20);
    GXSetTevKAlphaSel(0, 5);
    GXSetTevKAlphaSel(3, 18);
    GXSetTevColorS10(1, (GXColorS10){-100, 200, -300, 400});

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x80);
    if (!p) die("gc_ram_ptr failed");
    for (u32 i = 0; i < 0x80; i += 4) wr32be(p + i, 0);

    u32 off = 0;
    wr32be(p + off, 0xDEADBEEFu); off += 4;
    wr32be(p + off, gc_gx_tev_kcolor_ra[0]); off += 4;
    wr32be(p + off, gc_gx_tev_kcolor_bg[0]); off += 4;
    wr32be(p + off, gc_gx_tev_kcolor_ra[2]); off += 4;
    wr32be(p + off, gc_gx_tev_kcolor_bg[2]); off += 4;
    wr32be(p + off, gc_gx_tev_ksel[0]); off += 4;
    wr32be(p + off, gc_gx_tev_ksel[1]); off += 4;
    wr32be(p + off, gc_gx_tev_colors10_ra_last); off += 4;
    wr32be(p + off, gc_gx_tev_colors10_bg_last); off += 4;
    wr32be(p + off, gc_gx_bp_sent_not); off += 4;
}
