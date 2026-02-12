#include <stdint.h>
#include "src/sdk_port/gc_mem.h"
#include "src/sdk_port/sdk_state.h"

typedef uint32_t u32;

void GXSetTevSwapMode(u32 stage, u32 ras_sel, u32 tex_sel);
void GXSetTevSwapModeTable(u32 table, u32 red, u32 green, u32 blue, u32 alpha);

extern u32 gc_gx_teva[16];
extern u32 gc_gx_tev_ksel[8];
extern u32 gc_gx_bp_sent_not;

static inline void wr32be(volatile uint8_t *p, u32 v) {
    p[0]=(uint8_t)(v>>24); p[1]=(uint8_t)(v>>16);
    p[2]=(uint8_t)(v>>8);  p[3]=(uint8_t)(v);
}

int main(void) {
    gc_mem_set(0x80000000u, 0x01800000u, (uint8_t *)0x80000000u);
    gc_sdk_state_reset();

    volatile uint8_t *out = (volatile uint8_t *)0x80300000;
    for (u32 i = 0; i < 0x40; i++) out[i] = 0;

    // GXSetTevSwapMode — stage 0: ras_sel=1, tex_sel=2
    GXSetTevSwapMode(0, 1, 2);
    // GXSetTevSwapMode — stage 5: ras_sel=3, tex_sel=0
    GXSetTevSwapMode(5, 3, 0);

    // GXSetTevSwapModeTable — table 0: RGBR -> (0,1,2,0)
    GXSetTevSwapModeTable(0, 0, 1, 2, 0);
    // GXSetTevSwapModeTable — table 2: ABGR -> (3,2,1,0)
    GXSetTevSwapModeTable(2, 3, 2, 1, 0);

    u32 off = 0;
    wr32be(out + off, 0xDEADBEEFu); off += 4;
    wr32be(out + off, gc_gx_teva[0]); off += 4;
    wr32be(out + off, gc_gx_teva[5]); off += 4;
    wr32be(out + off, gc_gx_tev_ksel[0]); off += 4;
    wr32be(out + off, gc_gx_tev_ksel[1]); off += 4;
    wr32be(out + off, gc_gx_tev_ksel[4]); off += 4;
    wr32be(out + off, gc_gx_tev_ksel[5]); off += 4;
    wr32be(out + off, gc_gx_bp_sent_not); off += 4;
    // Total: 8 * 4 = 0x20

    while (1) { __asm__ volatile("nop"); }
    return 0;
}
