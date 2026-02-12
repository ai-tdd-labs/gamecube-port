#include <stdint.h>

typedef uint8_t u8;
typedef uint32_t u32;

static u32 s_gen_mode;
static u32 s_dirty_state;

static inline u32 set_field(u32 reg, u32 size, u32 shift, u32 v) {
    const u32 mask = ((1u << size) - 1u) << shift;
    return (reg & ~mask) | ((v << shift) & mask);
}

static void GXSetNumIndStages(u8 nIndStages) {
    s_gen_mode = set_field(s_gen_mode, 3u, 16u, (u32)(nIndStages & 7u));
    s_dirty_state |= 6u;
}

int main(void) {
    s_gen_mode = 0xA5A50000u;
    s_dirty_state = 0x00000008u;

    GXSetNumIndStages(3u);

    *(volatile u32 *)0x80300000 = 0xDEADBEEFu;
    *(volatile u32 *)0x80300004 = s_gen_mode;
    *(volatile u32 *)0x80300008 = s_dirty_state;
    while (1) {}
}
