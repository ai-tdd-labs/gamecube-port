#include <stdint.h>

typedef uint32_t u32;

u32 gc_gx_teva[16];
u32 gc_gx_tev_ksel[8];
u32 gc_gx_bp_sent_not;

static inline u32 set_field(u32 reg, u32 width, u32 shift, u32 val) {
    u32 mask;
    if (width >= 32u) {
        mask = 0xFFFFFFFFu;
    } else {
        mask = ((1u << width) - 1u) << shift;
    }
    return (reg & ~mask) | ((val << shift) & mask);
}

void GXSetTevSwapMode(u32 stage, u32 ras_sel, u32 tex_sel) {
    if (stage >= 16u) return;
    gc_gx_teva[stage] = set_field(gc_gx_teva[stage], 2, 0, ras_sel);
    gc_gx_teva[stage] = set_field(gc_gx_teva[stage], 2, 2, tex_sel);
    gc_gx_bp_sent_not = 0u;
}

void GXSetTevSwapModeTable(u32 table, u32 red, u32 green, u32 blue, u32 alpha) {
    if (table >= 4u) return;
    u32 idx0 = table * 2u;
    u32 idx1 = table * 2u + 1u;
    gc_gx_tev_ksel[idx0] = set_field(gc_gx_tev_ksel[idx0], 2, 0, red);
    gc_gx_tev_ksel[idx0] = set_field(gc_gx_tev_ksel[idx0], 2, 2, green);
    gc_gx_tev_ksel[idx1] = set_field(gc_gx_tev_ksel[idx1], 2, 0, blue);
    gc_gx_tev_ksel[idx1] = set_field(gc_gx_tev_ksel[idx1], 2, 2, alpha);
    gc_gx_bp_sent_not = 0u;
}
