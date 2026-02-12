#include <stdint.h>

typedef uint32_t u32;

static u32 s_iref;
static u32 s_last_ras_reg;
static u32 s_dirty_state;
static u32 s_bp_sent_not;

static inline u32 set_field(u32 reg, u32 size, u32 shift, u32 v) {
    const u32 mask = ((1u << size) - 1u) << shift;
    return (reg & ~mask) | ((v << shift) & mask);
}

static void GX_WRITE_RAS_REG(u32 v) {
    s_last_ras_reg = v;
}

static void GXSetIndTexOrder(u32 ind_stage, u32 tex_coord, u32 tex_map) {
    switch (ind_stage & 3u) {
    case 0u:
        s_iref = set_field(s_iref, 3u, 0u, tex_map & 7u);
        s_iref = set_field(s_iref, 3u, 3u, tex_coord & 7u);
        break;
    case 1u:
        s_iref = set_field(s_iref, 3u, 6u, tex_map & 7u);
        s_iref = set_field(s_iref, 3u, 9u, tex_coord & 7u);
        break;
    case 2u:
        s_iref = set_field(s_iref, 3u, 12u, tex_map & 7u);
        s_iref = set_field(s_iref, 3u, 15u, tex_coord & 7u);
        break;
    case 3u:
        s_iref = set_field(s_iref, 3u, 18u, tex_map & 7u);
        s_iref = set_field(s_iref, 3u, 21u, tex_coord & 7u);
        break;
    }
    GX_WRITE_RAS_REG(s_iref);
    s_dirty_state |= 3u;
    s_bp_sent_not = 0;
}

int main(void) {
    s_iref = 0x44000000u;
    s_last_ras_reg = 0;
    s_dirty_state = 0x00000008u;
    s_bp_sent_not = 1;

    GXSetIndTexOrder(1u, 2u, 3u);

    *(volatile u32 *)0x80300000 = 0xDEADBEEFu;
    *(volatile u32 *)0x80300004 = s_iref;
    *(volatile u32 *)0x80300008 = s_last_ras_reg;
    *(volatile u32 *)0x8030000C = s_dirty_state;
    *(volatile u32 *)0x80300010 = s_bp_sent_not;
    while (1) {}
}
