#include <stdint.h>

typedef uint32_t u32;

u32 gc_gx_tev_ksel[8];
u32 gc_gx_bp_sent_not;
u32 gc_gx_last_ras_reg;

static inline u32 set_field(u32 reg, u32 size, u32 shift, u32 v) {
    const u32 mask = ((1u << size) - 1u) << shift;
    return (reg & ~mask) | ((v << shift) & mask);
}

void oracle_GXSetTevSwapModeTable_reset(u32 s) {
    for (u32 i = 0; i < 8u; i++) {
        gc_gx_tev_ksel[i] = (s << (i & 7u)) ^ (0x01010101u * (i + 1u));
    }
    gc_gx_bp_sent_not = s ^ 0xA5A5A5A5u;
    gc_gx_last_ras_reg = s ^ 0x5A5A5A5Au;
}

void oracle_GXSetTevSwapModeTable(u32 table, u32 red, u32 green, u32 blue, u32 alpha) {
    if (table >= 4u) return;
    u32 idx0 = table * 2u;
    u32 idx1 = idx0 + 1u;

    gc_gx_tev_ksel[idx0] = set_field(gc_gx_tev_ksel[idx0], 2, 0, red);
    gc_gx_tev_ksel[idx0] = set_field(gc_gx_tev_ksel[idx0], 2, 2, green);
    gc_gx_last_ras_reg = gc_gx_tev_ksel[idx0];

    gc_gx_tev_ksel[idx1] = set_field(gc_gx_tev_ksel[idx1], 2, 0, blue);
    gc_gx_tev_ksel[idx1] = set_field(gc_gx_tev_ksel[idx1], 2, 2, alpha);
    gc_gx_last_ras_reg = gc_gx_tev_ksel[idx1];
    gc_gx_bp_sent_not = 0u;
}
