#include <stdint.h>
#include "src/sdk_port/gc_mem.h"
#include "src/sdk_port/sdk_state.h"

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

static inline void wr32be(volatile uint8_t *p, u32 v) {
    p[0]=(uint8_t)(v>>24); p[1]=(uint8_t)(v>>16);
    p[2]=(uint8_t)(v>>8);  p[3]=(uint8_t)(v);
}

int main(void) {
    gc_mem_set(0x80000000u, 0x01800000u, (uint8_t *)0x80000000u);
    gc_sdk_state_reset();

    volatile uint8_t *out = (volatile uint8_t *)0x80300000;
    for (u32 i = 0; i < 0x80; i++) out[i] = 0;

    // 1. GXSetTevKColor — id=0, color=(0x11, 0x22, 0x33, 0x44)
    GXSetTevKColor(0, (GXColor){0x11, 0x22, 0x33, 0x44});
    // 2. GXSetTevKColor — id=2, color=(0xAA, 0xBB, 0xCC, 0xDD)
    GXSetTevKColor(2, (GXColor){0xAA, 0xBB, 0xCC, 0xDD});

    // 3. GXSetTevKColorSel — stage 0: sel=12, stage 3: sel=20
    GXSetTevKColorSel(0, 12);
    GXSetTevKColorSel(3, 20);

    // 4. GXSetTevKAlphaSel — stage 0: sel=5, stage 3: sel=18
    GXSetTevKAlphaSel(0, 5);
    GXSetTevKAlphaSel(3, 18);

    // 5. GXSetTevColorS10 — id=1, color=(-100, 200, -300, 400)
    GXSetTevColorS10(1, (GXColorS10){-100, 200, -300, 400});

    u32 off = 0;
    wr32be(out + off, 0xDEADBEEFu); off += 4;

    // kcolor RA/BG for id 0 and 2
    wr32be(out + off, gc_gx_tev_kcolor_ra[0]); off += 4;
    wr32be(out + off, gc_gx_tev_kcolor_bg[0]); off += 4;
    wr32be(out + off, gc_gx_tev_kcolor_ra[2]); off += 4;
    wr32be(out + off, gc_gx_tev_kcolor_bg[2]); off += 4;

    // ksel[0] (covers stages 0,1), ksel[1] (covers stages 2,3)
    wr32be(out + off, gc_gx_tev_ksel[0]); off += 4;
    wr32be(out + off, gc_gx_tev_ksel[1]); off += 4;

    // colorS10 last
    wr32be(out + off, gc_gx_tev_colors10_ra_last); off += 4;
    wr32be(out + off, gc_gx_tev_colors10_bg_last); off += 4;

    // bp_sent_not
    wr32be(out + off, gc_gx_bp_sent_not); off += 4;

    // Total: 10 * 4 = 40 bytes = 0x28

    while (1) { __asm__ volatile("nop"); }
    return 0;
}
