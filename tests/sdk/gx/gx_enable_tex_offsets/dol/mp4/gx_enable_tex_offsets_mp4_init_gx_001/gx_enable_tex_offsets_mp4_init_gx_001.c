#include <stdint.h>

typedef uint32_t u32;
typedef uint8_t u8;

typedef struct { u8 _dummy; } GXFifoObj;
static GXFifoObj s_fifo_obj;

static u32 s_su_ts0[8];
static u32 s_bp_sent_not;

static inline u32 set_field(u32 reg, u32 size, u32 shift, u32 v) {
    const u32 mask = ((1u << size) - 1u) << shift;
    return (reg & ~mask) | ((v << shift) & mask);
}

static GXFifoObj *GXInit(void *base, u32 size) {
    u32 i;
    (void)base; (void)size;
    for (i = 0; i < 8; i++) s_su_ts0[i] = 0;
    s_bp_sent_not = 1;
    return &s_fifo_obj;
}

static void GXEnableTexOffsets(u32 coord, u8 line_enable, u8 point_enable) {
    if (coord >= 8) return;
    s_su_ts0[coord] = set_field(s_su_ts0[coord], 1, 18, (u32)(line_enable != 0));
    s_su_ts0[coord] = set_field(s_su_ts0[coord], 1, 19, (u32)(point_enable != 0));
    s_bp_sent_not = 0;
}

int main(void) {
    static u8 fifo[0x20000];
    GXInit(fifo, sizeof(fifo));

    GXEnableTexOffsets(0, 0, 0);

    *(volatile u32 *)0x80300000 = 0xDEADBEEFu;
    *(volatile u32 *)0x80300004 = s_su_ts0[0];
    *(volatile u32 *)0x80300008 = s_bp_sent_not;
    while (1) {}
}
