#include <stdint.h>

typedef uint32_t u32;

static u32 s_ind_tex_scale0;
static u32 s_ind_tex_scale1;
static u32 s_last_ras_reg;
static u32 s_bp_sent_not;

static inline u32 set_field(u32 reg, u32 size, u32 shift, u32 v) {
    const u32 mask = ((1u << size) - 1u) << shift;
    return (reg & ~mask) | ((v << shift) & mask);
}

static void GX_WRITE_RAS_REG(u32 v) {
    s_last_ras_reg = v;
}

static void GXSetIndTexCoordScale(u32 ind_state, u32 scale_s, u32 scale_t) {
    switch (ind_state & 3u) {
    case 0u:
        s_ind_tex_scale0 = set_field(s_ind_tex_scale0, 4u, 0u, scale_s & 0xFu);
        s_ind_tex_scale0 = set_field(s_ind_tex_scale0, 4u, 4u, scale_t & 0xFu);
        s_ind_tex_scale0 = set_field(s_ind_tex_scale0, 8u, 24u, 0x25u);
        GX_WRITE_RAS_REG(s_ind_tex_scale0);
        break;
    case 1u:
        s_ind_tex_scale0 = set_field(s_ind_tex_scale0, 4u, 8u, scale_s & 0xFu);
        s_ind_tex_scale0 = set_field(s_ind_tex_scale0, 4u, 12u, scale_t & 0xFu);
        s_ind_tex_scale0 = set_field(s_ind_tex_scale0, 8u, 24u, 0x25u);
        GX_WRITE_RAS_REG(s_ind_tex_scale0);
        break;
    case 2u:
        s_ind_tex_scale1 = set_field(s_ind_tex_scale1, 4u, 0u, scale_s & 0xFu);
        s_ind_tex_scale1 = set_field(s_ind_tex_scale1, 4u, 4u, scale_t & 0xFu);
        s_ind_tex_scale1 = set_field(s_ind_tex_scale1, 8u, 24u, 0x26u);
        GX_WRITE_RAS_REG(s_ind_tex_scale1);
        break;
    case 3u:
        s_ind_tex_scale1 = set_field(s_ind_tex_scale1, 4u, 8u, scale_s & 0xFu);
        s_ind_tex_scale1 = set_field(s_ind_tex_scale1, 4u, 12u, scale_t & 0xFu);
        s_ind_tex_scale1 = set_field(s_ind_tex_scale1, 8u, 24u, 0x26u);
        GX_WRITE_RAS_REG(s_ind_tex_scale1);
        break;
    }
    s_bp_sent_not = 0;
}

int main(void) {
    s_ind_tex_scale0 = 0x12340000u;
    s_ind_tex_scale1 = 0x56780000u;
    s_last_ras_reg = 0;
    s_bp_sent_not = 1;

    GXSetIndTexCoordScale(2u, 1u, 1u);

    *(volatile u32 *)0x80300000 = 0xDEADBEEFu;
    *(volatile u32 *)0x80300004 = s_ind_tex_scale0;
    *(volatile u32 *)0x80300008 = s_ind_tex_scale1;
    *(volatile u32 *)0x8030000C = s_last_ras_reg;
    *(volatile u32 *)0x80300010 = s_bp_sent_not;
    while (1) {}
}
