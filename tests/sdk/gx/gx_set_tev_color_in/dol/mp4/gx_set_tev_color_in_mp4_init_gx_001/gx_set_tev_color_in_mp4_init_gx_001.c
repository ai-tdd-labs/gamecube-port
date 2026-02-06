#include <stdint.h>

typedef uint32_t u32;
typedef uint8_t u8;

typedef struct { u8 _dummy; } GXFifoObj;
static GXFifoObj s_fifo_obj;

static u32 s_tevc[16];
static u32 s_bp_sent_not;

static inline u32 set_field(u32 reg, u32 size, u32 shift, u32 v) {
    const u32 mask = ((1u << size) - 1u) << shift;
    return (reg & ~mask) | ((v << shift) & mask);
}

static void GXSetTevColorIn(u32 stage, u32 a, u32 b, u32 c, u32 d) {
    u32 reg = s_tevc[stage];
    reg = set_field(reg, 4, 12, a);
    reg = set_field(reg, 4, 8, b);
    reg = set_field(reg, 4, 4, c);
    reg = set_field(reg, 4, 0, d);
    s_tevc[stage] = reg;
    s_bp_sent_not = 0;
}

static GXFifoObj *GXInit(void *base, u32 size) {
    (void)base; (void)size;
    u32 i;
    for (i = 0; i < 16; i++) s_tevc[i] = 0;
    s_bp_sent_not = 1;
    return &s_fifo_obj;
}

enum { GX_TEVSTAGE0 = 0 };
enum { GX_CC_ZERO = 15, GX_CC_TEXC = 8, GX_CC_RASC = 10 };

int main(void) {
    static u8 fifo[0x20000];
    GXInit(fifo, sizeof(fifo));

    GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_TEXC, GX_CC_RASC, GX_CC_ZERO);

    *(volatile u32 *)0x80300000 = 0xDEADBEEFu;
    *(volatile u32 *)0x80300004 = s_tevc[0];
    *(volatile u32 *)0x80300008 = s_bp_sent_not;
    while (1) {}
}
