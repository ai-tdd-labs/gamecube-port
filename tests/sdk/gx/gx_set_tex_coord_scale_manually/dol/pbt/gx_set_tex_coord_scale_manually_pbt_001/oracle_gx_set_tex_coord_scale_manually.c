#include <stdint.h>

typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;

u32 gc_gx_su_ts0[8];
u32 gc_gx_su_ts1[8];
u32 gc_gx_tcs_man_enab;
u32 gc_gx_bp_sent_not;
u32 gc_gx_last_ras_reg;

static inline u32 set_field(u32 reg, u32 size, u32 shift, u32 v) {
    const u32 mask = ((1u << size) - 1u) << shift;
    return (reg & ~mask) | ((v << shift) & mask);
}

static inline void write_ras(u32 v) {
    gc_gx_last_ras_reg = v;
}

void oracle_GXSetTexCoordScaleManually_reset(void) {
    for (u32 i = 0; i < 8u; i++) {
        gc_gx_su_ts0[i] = 0x70000000u ^ (i * 0x1111u);
        gc_gx_su_ts1[i] = 0x71000000u ^ (i * 0x2222u);
    }
    gc_gx_tcs_man_enab = 0;
    gc_gx_bp_sent_not = 1u;
    gc_gx_last_ras_reg = 0;
}

void oracle_GXSetTexCoordScaleManually(u32 coord, u8 enable, u16 ss, u16 ts) {
    if (coord >= 8u) return;

    gc_gx_tcs_man_enab = (gc_gx_tcs_man_enab & ~(1u << coord)) | ((u32)(enable != 0) << coord);
    if (enable != 0) {
        gc_gx_su_ts0[coord] = set_field(gc_gx_su_ts0[coord], 16, 0, (u32)(u16)(ss - 1u));
        gc_gx_su_ts1[coord] = set_field(gc_gx_su_ts1[coord], 16, 0, (u32)(u16)(ts - 1u));
        write_ras(gc_gx_su_ts0[coord]);
        write_ras(gc_gx_su_ts1[coord]);
        gc_gx_bp_sent_not = 0;
    }
}
