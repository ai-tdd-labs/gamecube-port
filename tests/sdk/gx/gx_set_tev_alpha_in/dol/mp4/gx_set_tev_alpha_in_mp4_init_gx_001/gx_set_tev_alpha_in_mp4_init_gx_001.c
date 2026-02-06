#include <stdint.h>

typedef uint32_t u32;
typedef uint8_t u8;

typedef struct { u8 _dummy; } GXFifoObj;
static GXFifoObj s_fifo_obj;

static u32 s_teva[16];
static u32 s_bp_sent_not;

static inline u32 set_field(u32 reg, u32 size, u32 shift, u32 v) {
    const u32 mask = ((1u << size) - 1u) << shift;
    return (reg & ~mask) | ((v << shift) & mask);
}

static void GXSetTevAlphaIn(u32 stage, u32 a, u32 b, u32 c, u32 d) {
    u32 reg = s_teva[stage];
    reg = set_field(reg, 3, 13, a);
    reg = set_field(reg, 3, 10, b);
    reg = set_field(reg, 3, 7, c);
    reg = set_field(reg, 3, 4, d);
    s_teva[stage] = reg;
    s_bp_sent_not = 0;
}

static GXFifoObj *GXInit(void *base, u32 size) {
    (void)base; (void)size;
    u32 i;
    for (i = 0; i < 16; i++) s_teva[i] = 0;
    s_bp_sent_not = 1;
    return &s_fifo_obj;
}

enum { GX_TEVSTAGE0 = 0 };
enum { GX_CA_ZERO = 7, GX_CA_TEXA = 4, GX_CA_RASA = 5 };

int main(void) {
    static u8 fifo[0x20000];
    GXInit(fifo, sizeof(fifo));

    GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_TEXA, GX_CA_RASA, GX_CA_ZERO);

    *(volatile u32 *)0x80300000 = 0xDEADBEEFu;
    *(volatile u32 *)0x80300004 = s_teva[0];
    *(volatile u32 *)0x80300008 = s_bp_sent_not;
    while (1) {}
}
