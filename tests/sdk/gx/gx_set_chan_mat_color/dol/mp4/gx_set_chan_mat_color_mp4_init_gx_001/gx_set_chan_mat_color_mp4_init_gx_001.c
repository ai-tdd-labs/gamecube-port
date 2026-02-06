#include <stdint.h>

typedef uint32_t u32;
typedef uint8_t u8;

typedef struct { u8 r, g, b, a; } GXColor;

typedef enum {
    GX_COLOR0 = 0,
    GX_COLOR1 = 1,
    GX_ALPHA0 = 2,
    GX_ALPHA1 = 3,
    GX_COLOR0A0 = 4,
    GX_COLOR1A1 = 5,
} GXChannelID;

static u32 s_xf_regs[32];
static u32 s_mat_color[2];
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

static void GXSetChanMatColor(GXChannelID chan, GXColor mat_color) {
    u32 reg = 0;
    u32 colIdx = 0;
    u32 alpha;

    switch (chan) {
        case GX_COLOR0:
            alpha = s_mat_color[0] & 0xFF;
            reg = set_field(reg, 8, 0, alpha);
            reg = set_field(reg, 8, 8, mat_color.b);
            reg = set_field(reg, 8, 16, mat_color.g);
            reg = set_field(reg, 8, 24, mat_color.r);
            colIdx = 0;
            break;
        case GX_COLOR1:
            alpha = s_mat_color[1] & 0xFF;
            reg = set_field(reg, 8, 0, alpha);
            reg = set_field(reg, 8, 8, mat_color.b);
            reg = set_field(reg, 8, 16, mat_color.g);
            reg = set_field(reg, 8, 24, mat_color.r);
            colIdx = 1;
            break;
        case GX_ALPHA0:
            reg = s_mat_color[0];
            reg = set_field(reg, 8, 0, mat_color.a);
            colIdx = 0;
            break;
        case GX_ALPHA1:
            reg = s_mat_color[1];
            reg = set_field(reg, 8, 0, mat_color.a);
            colIdx = 1;
            break;
        case GX_COLOR0A0:
            reg = set_field(reg, 8, 0, mat_color.a);
            reg = set_field(reg, 8, 8, mat_color.b);
            reg = set_field(reg, 8, 16, mat_color.g);
            reg = set_field(reg, 8, 24, mat_color.r);
            colIdx = 0;
            break;
        case GX_COLOR1A1:
            reg = set_field(reg, 8, 0, mat_color.a);
            reg = set_field(reg, 8, 8, mat_color.b);
            reg = set_field(reg, 8, 16, mat_color.g);
            reg = set_field(reg, 8, 24, mat_color.r);
            colIdx = 1;
            break;
        default:
            return;
    }

    GX_WRITE_XF_REG(colIdx + 12, reg);
    s_bp_sent_not = 1;
    s_mat_color[colIdx] = reg;
}

int main(void) {
    u32 i;
    for (i = 0; i < 32; i++) s_xf_regs[i] = 0;
    s_mat_color[0] = 0;
    s_mat_color[1] = 0;
    s_bp_sent_not = 0;

    GXColor white = {255, 255, 255, 255};
    GXSetChanMatColor(GX_COLOR0A0, white);

    *(volatile u32 *)0x80300000 = 0xDEADBEEFu;
    *(volatile u32 *)0x80300004 = s_xf_regs[12];
    *(volatile u32 *)0x80300008 = s_mat_color[0];
    *(volatile u32 *)0x8030000C = s_bp_sent_not;
    while (1) {}
}
