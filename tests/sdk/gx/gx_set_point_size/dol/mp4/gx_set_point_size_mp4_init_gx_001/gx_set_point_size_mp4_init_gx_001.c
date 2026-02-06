#include <stdint.h>

typedef uint32_t u32;
typedef uint8_t u8;

typedef struct { u8 _dummy; } GXFifoObj;
static GXFifoObj s_fifo_obj;

static u32 s_lp_size;
static u32 s_bp_sent_not;

static inline u32 set_field(u32 reg, u32 size, u32 shift, u32 v) {
    const u32 mask = ((1u << size) - 1u) << shift;
    return (reg & ~mask) | ((v << shift) & mask);
}

static GXFifoObj *GXInit(void *base, u32 size) {
    (void)base; (void)size;
    s_lp_size = 0;
    s_bp_sent_not = 1;
    return &s_fifo_obj;
}

static void GXSetPointSize(u8 pointSize, u32 texOffsets) {
    s_lp_size = set_field(s_lp_size, 8, 8, (u32)pointSize);
    s_lp_size = set_field(s_lp_size, 3, 19, texOffsets & 7u);
    s_bp_sent_not = 0;
}

int main(void) {
    static u8 fifo[0x20000];
    GXInit(fifo, sizeof(fifo));

    GXSetPointSize(6, 0);

    *(volatile u32 *)0x80300000 = 0xDEADBEEFu;
    *(volatile u32 *)0x80300004 = s_lp_size;
    *(volatile u32 *)0x80300008 = s_bp_sent_not;
    while (1) {}
}
