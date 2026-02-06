#include <stdint.h>

typedef uint32_t u32;
typedef uint8_t u8;

static u32 s_gen_mode;
static u32 s_dirty_state;

static inline u32 set_field(u32 reg, u32 size, u32 shift, u32 v) {
    const u32 mask = ((1u << size) - 1u) << shift;
    return (reg & ~mask) | ((v << shift) & mask);
}

static void GXSetNumTevStages(u8 nStages) {
    /* Matches GXTev.c: shift=10 size=4, store nStages-1, dirtyState |= 4 */
    s_gen_mode = set_field(s_gen_mode, 4, 10, (u32)(nStages - 1u));
    s_dirty_state |= 4u;
}

int main(void) {
    s_gen_mode = 0;
    s_dirty_state = 0;

    GXSetNumTevStages(1);

    *(volatile u32 *)0x80300000 = 0xDEADBEEFu;
    *(volatile u32 *)0x80300004 = s_gen_mode;
    *(volatile u32 *)0x80300008 = s_dirty_state;
    while (1) {}
}
