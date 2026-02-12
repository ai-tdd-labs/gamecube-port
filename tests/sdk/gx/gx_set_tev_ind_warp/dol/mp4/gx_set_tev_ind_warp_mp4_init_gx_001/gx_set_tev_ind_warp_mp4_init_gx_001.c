#include <stdint.h>

typedef uint8_t u8;
typedef uint32_t u32;

static u32 s_last_ras_reg;
static u32 s_bp_sent_not;

static inline u32 set_field(u32 reg, u32 size, u32 shift, u32 v) {
    const u32 mask = ((1u << size) - 1u) << shift;
    return (reg & ~mask) | ((v << shift) & mask);
}

static void GX_WRITE_RAS_REG(u32 v) {
    s_last_ras_reg = v;
}

static void GXSetTevIndirect(u32 tev_stage, u32 ind_stage, u32 format, u32 bias_sel, u32 matrix_sel, u32 wrap_s, u32 wrap_t,
                             u32 add_prev, u32 utc_lod, u32 alpha_sel) {
    u32 reg = 0;
    reg = set_field(reg, 2u, 0u, ind_stage & 3u);
    reg = set_field(reg, 2u, 2u, format & 3u);
    reg = set_field(reg, 3u, 4u, bias_sel & 7u);
    reg = set_field(reg, 2u, 7u, alpha_sel & 3u);
    reg = set_field(reg, 4u, 9u, matrix_sel & 0xFu);
    reg = set_field(reg, 3u, 13u, wrap_s & 7u);
    reg = set_field(reg, 3u, 16u, wrap_t & 7u);
    reg = set_field(reg, 1u, 19u, utc_lod & 1u);
    reg = set_field(reg, 1u, 20u, add_prev & 1u);
    reg = set_field(reg, 8u, 24u, (tev_stage + 16u) & 0xFFu);
    GX_WRITE_RAS_REG(reg);
    s_bp_sent_not = 0;
}

static void GXSetTevIndWarp(u32 tev_stage, u32 ind_stage, u8 signed_offset, u8 replace_mode, u32 matrix_sel) {
    const u32 GX_ITF_8 = 0u;
    const u32 GX_ITB_NONE = 0u;
    const u32 GX_ITB_STU = 7u;
    const u32 GX_ITW_OFF = 0u;
    const u32 GX_ITW_0 = 6u;
    u32 wrap = (replace_mode != 0u) ? GX_ITW_0 : GX_ITW_OFF;
    GXSetTevIndirect(tev_stage, ind_stage, GX_ITF_8, (signed_offset != 0u) ? GX_ITB_STU : GX_ITB_NONE, matrix_sel,
                     wrap, wrap, 0u, 0u, 0u);
}

int main(void) {
    s_last_ras_reg = 0;
    s_bp_sent_not = 1;
    GXSetTevIndWarp(2u, 1u, 1u, 1u, 2u);

    *(volatile u32 *)0x80300000 = 0xDEADBEEFu;
    *(volatile u32 *)0x80300004 = s_last_ras_reg;
    *(volatile u32 *)0x80300008 = s_bp_sent_not;
    while (1) {}
}
