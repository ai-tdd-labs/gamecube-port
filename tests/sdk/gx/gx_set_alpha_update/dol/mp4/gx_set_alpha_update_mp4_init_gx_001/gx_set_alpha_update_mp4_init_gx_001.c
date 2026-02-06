#include <stdint.h>

typedef uint32_t u32;

typedef enum { GX_FALSE = 0, GX_TRUE = 1 } GXBool;

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

static void GXSetAlphaUpdate(GXBool update_enable) {
    s_cmode0 = set_field(s_cmode0, 1, 4, (u32)update_enable);
    GX_WRITE_RAS_REG(s_cmode0);
    s_bp_sent_not = 0;
}

int main(void) {
    s_cmode0 = 0;
    s_last_ras_reg = 0;
    s_bp_sent_not = 1;

    GXSetAlphaUpdate(GX_TRUE);

    *(volatile u32 *)0x80300000 = 0xDEADBEEFu;
    *(volatile u32 *)0x80300004 = s_cmode0;
    *(volatile u32 *)0x80300008 = s_last_ras_reg;
    *(volatile u32 *)0x8030000C = s_bp_sent_not;
    while (1) {}
}
