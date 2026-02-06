#include <stdint.h>

typedef int32_t s32;
typedef uint32_t u32;
typedef uint8_t u8;

typedef struct { u8 _dummy; } GXFifoObj;
static GXFifoObj s_fifo_obj;

static u32 s_reg;
static u32 s_bp_sent_not;

static inline u32 set_field(u32 reg, u32 size, u32 shift, u32 v) {
    const u32 mask = ((1u << size) - 1u) << shift;
    return (reg & ~mask) | ((v << shift) & mask);
}

static GXFifoObj *GXInit(void *base, u32 size) {
    (void)base; (void)size;
    s_reg = 0;
    s_bp_sent_not = 1;
    return &s_fifo_obj;
}

static void GXSetScissorBoxOffset(s32 x_off, s32 y_off) {
    u32 reg = 0;
    u32 hx = (u32)(x_off + 342) >> 1;
    u32 hy = (u32)(y_off + 342) >> 1;
    reg = set_field(reg, 10, 0, hx);
    reg = set_field(reg, 10, 10, hy);
    reg = set_field(reg, 8, 24, 0x59u);
    s_reg = reg;
    s_bp_sent_not = 0;
}

int main(void) {
    static u8 fifo[0x20000];
    GXInit(fifo, sizeof(fifo));

    GXSetScissorBoxOffset(0, 0);

    *(volatile u32 *)0x80300000 = 0xDEADBEEFu;
    *(volatile u32 *)0x80300004 = s_reg;
    *(volatile u32 *)0x80300008 = s_bp_sent_not;
    while (1) {}
}
