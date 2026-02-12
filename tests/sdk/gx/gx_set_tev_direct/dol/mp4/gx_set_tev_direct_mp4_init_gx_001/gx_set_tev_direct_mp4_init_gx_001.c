#include <stdint.h>

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

static void GXSetTevDirect(u32 tev_stage) {
    u32 reg = 0;
    reg = set_field(reg, 8u, 24u, (tev_stage + 16u) & 0xFFu);
    GX_WRITE_RAS_REG(reg);
    s_bp_sent_not = 0;
}

int main(void) {
    s_last_ras_reg = 0;
    s_bp_sent_not = 1;

    GXSetTevDirect(5u);

    *(volatile u32 *)0x80300000 = 0xDEADBEEFu;
    *(volatile u32 *)0x80300004 = s_last_ras_reg;
    *(volatile u32 *)0x80300008 = s_bp_sent_not;
    while (1) {}
}
