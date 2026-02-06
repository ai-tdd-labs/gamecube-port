#include <stdint.h>
typedef uint32_t u32;
typedef uint8_t u8;

typedef struct { u8 _dummy; } GXFifoObj;
static GXFifoObj s_fifo_obj;

static u32 s_su_scis0;
static u32 s_su_scis1;
static u32 s_bp_sent_not;

static inline u32 set_field(u32 reg, u32 size, u32 shift, u32 v) {
    const u32 mask = ((1u << size) - 1u) << shift;
    return (reg & ~mask) | ((v << shift) & mask);
}

static GXFifoObj *GXInit(void *base, u32 size) {
    (void)base; (void)size;
    s_su_scis0 = 0;
    s_su_scis1 = 0;
    s_bp_sent_not = 0;
    return &s_fifo_obj;
}

static void GXSetScissor(u32 left, u32 top, u32 wd, u32 ht) {
    u32 tp = top + 342u;
    u32 lf = left + 342u;
    u32 bm = tp + ht - 1u;
    u32 rt = lf + wd - 1u;

    s_su_scis0 = set_field(s_su_scis0, 11, 0, tp);
    s_su_scis0 = set_field(s_su_scis0, 11, 12, lf);
    s_su_scis1 = set_field(s_su_scis1, 11, 0, bm);
    s_su_scis1 = set_field(s_su_scis1, 11, 12, rt);

    s_bp_sent_not = 0;
}

int main(void) {
    static u8 fifo[0x20000];
    GXInit(fifo, sizeof(fifo));
    GXSetScissor(0, 0, 640, 448);

    *(volatile u32 *)0x80300000 = 0xDEADBEEFu;
    *(volatile u32 *)0x80300004 = s_su_scis0;
    *(volatile u32 *)0x80300008 = s_su_scis1;
    *(volatile u32 *)0x8030000C = s_bp_sent_not;
    while (1) {}
}
