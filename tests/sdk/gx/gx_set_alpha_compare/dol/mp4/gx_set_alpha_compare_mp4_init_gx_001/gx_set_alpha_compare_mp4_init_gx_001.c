#include <stdint.h>

typedef uint32_t u32;
typedef uint8_t u8;

typedef enum { GX_ALWAYS = 7 } GXCompare;
typedef enum { GX_AOP_AND = 0 } GXAlphaOp;

static u32 s_last_ras_reg;
static u32 s_bp_sent_not;

static inline u32 set_field(u32 reg, u32 size, u32 shift, u32 v) {
    const u32 mask = ((1u << size) - 1u) << shift;
    return (reg & ~mask) | ((v << shift) & mask);
}

static void GX_WRITE_RAS_REG(u32 v) {
    s_last_ras_reg = v;
}

static void GXSetAlphaCompare(GXCompare comp0, u8 ref0, GXAlphaOp op, GXCompare comp1, u8 ref1) {
    u32 reg = 0;
    reg = set_field(reg, 8, 0, (u32)ref0);
    reg = set_field(reg, 8, 8, (u32)ref1);
    reg = set_field(reg, 3, 16, (u32)comp0);
    reg = set_field(reg, 3, 19, (u32)comp1);
    reg = set_field(reg, 2, 22, (u32)op);
    reg = set_field(reg, 8, 24, 0xF3u);

    GX_WRITE_RAS_REG(reg);
    s_bp_sent_not = 0;
}

int main(void) {
    s_last_ras_reg = 0;
    s_bp_sent_not = 1;

    GXSetAlphaCompare(GX_ALWAYS, 0, GX_AOP_AND, GX_ALWAYS, 0);

    *(volatile u32 *)0x80300000 = 0xDEADBEEFu;
    *(volatile u32 *)0x80300004 = s_last_ras_reg;
    *(volatile u32 *)0x80300008 = s_bp_sent_not;
    while (1) {}
}
