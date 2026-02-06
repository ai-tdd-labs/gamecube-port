#include <stdint.h>

typedef uint32_t u32;
typedef uint8_t u8;

typedef enum { GX_BM_NONE = 0, GX_BM_BLEND = 1, GX_BM_LOGIC = 2, GX_BM_SUBTRACT = 3 } GXBlendMode;
typedef enum { GX_BL_SRCALPHA = 4, GX_BL_INVSRCALPHA = 5 } GXBlendFactor;
typedef enum { GX_LO_CLEAR = 0 } GXLogicOp;

static u32 s_cmode0;
static u32 s_last_ras_reg;
static u32 s_bp_sent_not;

static inline u32 set_field(u32 reg, u32 size, u32 shift, u32 v) {
    const u32 mask = ((1u << size) - 1u) << shift;
    return (reg & ~mask) | ((v << shift) & mask);
}

static void GX_WRITE_RAS_REG(u32 v) {
    s_last_ras_reg = v;
}

static void GXSetBlendMode(GXBlendMode type, GXBlendFactor src_factor, GXBlendFactor dst_factor, GXLogicOp op) {
    s_cmode0 = set_field(s_cmode0, 1, 0, (type == GX_BM_BLEND || type == GX_BM_SUBTRACT) ? 1u : 0u);
    s_cmode0 = set_field(s_cmode0, 1, 11, (type == GX_BM_SUBTRACT) ? 1u : 0u);
    s_cmode0 = set_field(s_cmode0, 1, 1, (type == GX_BM_LOGIC) ? 1u : 0u);
    s_cmode0 = set_field(s_cmode0, 4, 12, (u32)op);
    s_cmode0 = set_field(s_cmode0, 3, 8, (u32)src_factor);
    s_cmode0 = set_field(s_cmode0, 3, 5, (u32)dst_factor);
    s_cmode0 = set_field(s_cmode0, 8, 24, 0x41u);
    GX_WRITE_RAS_REG(s_cmode0);
    s_bp_sent_not = 0;
}

int main(void) {
    s_cmode0 = 0;
    s_last_ras_reg = 0;
    s_bp_sent_not = 1;

    GXSetBlendMode(GX_BM_NONE, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);

    *(volatile u32 *)0x80300000 = 0xDEADBEEFu;
    *(volatile u32 *)0x80300004 = s_cmode0;
    *(volatile u32 *)0x80300008 = s_last_ras_reg;
    *(volatile u32 *)0x8030000C = s_bp_sent_not;
    while (1) {}
}
