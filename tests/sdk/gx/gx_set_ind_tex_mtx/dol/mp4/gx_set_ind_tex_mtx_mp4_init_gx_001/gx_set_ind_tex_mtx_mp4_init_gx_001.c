#include <stdint.h>

typedef int8_t s8;
typedef int32_t s32;
typedef uint32_t u32;
typedef float f32;

static u32 s_ind_mtx_reg0;
static u32 s_ind_mtx_reg1;
static u32 s_ind_mtx_reg2;
static u32 s_last_ras_reg;
static u32 s_bp_sent_not;

static inline u32 set_field(u32 reg, u32 size, u32 shift, u32 v) {
    const u32 mask = ((1u << size) - 1u) << shift;
    return (reg & ~mask) | ((v << shift) & mask);
}

static void GX_WRITE_RAS_REG(u32 v) {
    s_last_ras_reg = v;
}

static void GXSetIndTexMtx(u32 mtx_id, f32 offset[2][3], s8 scale_exp) {
    u32 id;
    s32 mtx[6];
    s32 sc = (s32)scale_exp + 0x11;
    u32 reg;

    switch (mtx_id) {
    case 1u:
    case 2u:
    case 3u: id = mtx_id - 1u; break;
    case 5u:
    case 6u:
    case 7u: id = mtx_id - 5u; break;
    case 9u:
    case 10u:
    case 11u: id = mtx_id - 9u; break;
    default: id = 0u; break;
    }

    mtx[0] = ((s32)(1024.0f * offset[0][0])) & 0x7FF;
    mtx[1] = ((s32)(1024.0f * offset[1][0])) & 0x7FF;
    reg = 0;
    reg = set_field(reg, 11u, 0u, (u32)mtx[0]);
    reg = set_field(reg, 11u, 11u, (u32)mtx[1]);
    reg = set_field(reg, 2u, 22u, (u32)sc & 3u);
    reg = set_field(reg, 8u, 24u, id * 3u + 6u);
    s_ind_mtx_reg0 = reg;
    GX_WRITE_RAS_REG(reg);

    mtx[2] = ((s32)(1024.0f * offset[0][1])) & 0x7FF;
    mtx[3] = ((s32)(1024.0f * offset[1][1])) & 0x7FF;
    reg = 0;
    reg = set_field(reg, 11u, 0u, (u32)mtx[2]);
    reg = set_field(reg, 11u, 11u, (u32)mtx[3]);
    reg = set_field(reg, 2u, 22u, ((u32)sc >> 2) & 3u);
    reg = set_field(reg, 8u, 24u, id * 3u + 7u);
    s_ind_mtx_reg1 = reg;
    GX_WRITE_RAS_REG(reg);

    mtx[4] = ((s32)(1024.0f * offset[0][2])) & 0x7FF;
    mtx[5] = ((s32)(1024.0f * offset[1][2])) & 0x7FF;
    reg = 0;
    reg = set_field(reg, 11u, 0u, (u32)mtx[4]);
    reg = set_field(reg, 11u, 11u, (u32)mtx[5]);
    reg = set_field(reg, 2u, 22u, ((u32)sc >> 4) & 3u);
    reg = set_field(reg, 8u, 24u, id * 3u + 8u);
    s_ind_mtx_reg2 = reg;
    GX_WRITE_RAS_REG(reg);

    s_bp_sent_not = 0;
}

int main(void) {
    f32 off[2][3] = {
        { 0.125f, -0.25f, 0.5f },
        { 0.75f, -0.5f, 0.25f },
    };
    s_ind_mtx_reg0 = 0;
    s_ind_mtx_reg1 = 0;
    s_ind_mtx_reg2 = 0;
    s_last_ras_reg = 0;
    s_bp_sent_not = 1;

    GXSetIndTexMtx(1u, off, 2);

    *(volatile u32 *)0x80300000 = 0xDEADBEEFu;
    *(volatile u32 *)0x80300004 = s_ind_mtx_reg0;
    *(volatile u32 *)0x80300008 = s_ind_mtx_reg1;
    *(volatile u32 *)0x8030000C = s_ind_mtx_reg2;
    *(volatile u32 *)0x80300010 = s_last_ras_reg;
    *(volatile u32 *)0x80300014 = s_bp_sent_not;
    while (1) {}
}
