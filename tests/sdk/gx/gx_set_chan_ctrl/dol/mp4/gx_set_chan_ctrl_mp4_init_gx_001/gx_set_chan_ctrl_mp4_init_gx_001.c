#include <stdint.h>

typedef uint32_t u32;
typedef uint8_t u8;

typedef enum {
    GX_COLOR0 = 0,
    GX_COLOR1 = 1,
    GX_ALPHA0 = 2,
    GX_ALPHA1 = 3,
    GX_COLOR0A0 = 4,
    GX_COLOR1A1 = 5,
} GXChannelID;

typedef enum { GX_FALSE = 0, GX_TRUE = 1 } GXBool;
typedef enum { GX_SRC_REG = 0, GX_SRC_VTX = 1 } GXColorSrc;
typedef enum { GX_DF_NONE = 0 } GXDiffuseFn;
typedef enum { GX_AF_NONE = 0, GX_AF_SPOT = 2 } GXAttnFn;

enum {
    GX_LIGHT0 = 1u << 0,
    GX_LIGHT1 = 1u << 1,
    GX_LIGHT2 = 1u << 2,
    GX_LIGHT3 = 1u << 3,
    GX_LIGHT4 = 1u << 4,
    GX_LIGHT5 = 1u << 5,
    GX_LIGHT6 = 1u << 6,
    GX_LIGHT7 = 1u << 7,
};

static u32 s_xf_regs[32];
static u32 s_bp_sent_not;

static inline u32 set_field(u32 reg, u32 size, u32 shift, u32 v) {
    const u32 mask = ((1u << size) - 1u) << shift;
    return (reg & ~mask) | ((v << shift) & mask);
}

static void GX_WRITE_XF_REG(u32 idx, u32 v) {
    if (idx < (sizeof(s_xf_regs) / sizeof(s_xf_regs[0]))) {
        s_xf_regs[idx] = v;
    }
}

static void GXSetChanCtrl(GXChannelID chan, GXBool enable, GXColorSrc amb_src, GXColorSrc mat_src,
                          u32 light_mask, GXDiffuseFn diff_fn, GXAttnFn attn_fn) {
    u32 reg;
    u32 idx;

    if (chan == GX_COLOR0A0) idx = 0;
    else if (chan == GX_COLOR1A1) idx = 1;
    else idx = (u32)chan;

    reg = 0;
    reg = set_field(reg, 1, 1, (u32)enable);
    reg = set_field(reg, 1, 0, (u32)mat_src);
    reg = set_field(reg, 1, 6, (u32)amb_src);
    reg = set_field(reg, 1, 2, (light_mask & GX_LIGHT0) ? 1u : 0u);
    reg = set_field(reg, 1, 3, (light_mask & GX_LIGHT1) ? 1u : 0u);
    reg = set_field(reg, 1, 4, (light_mask & GX_LIGHT2) ? 1u : 0u);
    reg = set_field(reg, 1, 5, (light_mask & GX_LIGHT3) ? 1u : 0u);
    reg = set_field(reg, 1, 11, (light_mask & GX_LIGHT4) ? 1u : 0u);
    reg = set_field(reg, 1, 12, (light_mask & GX_LIGHT5) ? 1u : 0u);
    reg = set_field(reg, 1, 13, (light_mask & GX_LIGHT6) ? 1u : 0u);
    reg = set_field(reg, 1, 14, (light_mask & GX_LIGHT7) ? 1u : 0u);
    reg = set_field(reg, 2, 7, (attn_fn == 0) ? 0u : (u32)diff_fn);
    reg = set_field(reg, 1, 9, (attn_fn != 2) ? 1u : 0u);
    reg = set_field(reg, 1, 10, (attn_fn != 0) ? 1u : 0u);

    GX_WRITE_XF_REG(idx + 14, reg);
    s_bp_sent_not = 1;

    if (chan == GX_COLOR0A0) {
        GX_WRITE_XF_REG(16, reg);
    } else if (chan == GX_COLOR1A1) {
        GX_WRITE_XF_REG(17, reg);
    }
}

int main(void) {
    u32 i;
    for (i = 0; i < 32; i++) s_xf_regs[i] = 0;
    s_bp_sent_not = 0;

    GXSetChanCtrl(GX_COLOR0A0, GX_FALSE, GX_SRC_REG, GX_SRC_VTX, 0, GX_DF_NONE, GX_AF_NONE);

    *(volatile u32 *)0x80300000 = 0xDEADBEEFu;
    *(volatile u32 *)0x80300004 = s_xf_regs[14];
    *(volatile u32 *)0x80300008 = s_xf_regs[16];
    *(volatile u32 *)0x8030000C = s_bp_sent_not;
    while (1) {}
}
