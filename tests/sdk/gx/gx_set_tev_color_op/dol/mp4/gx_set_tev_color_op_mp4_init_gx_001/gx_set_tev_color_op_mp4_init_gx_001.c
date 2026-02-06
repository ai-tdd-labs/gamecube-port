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

static void GXSetTevColorOp(u32 stage, u32 op, u32 bias, u32 scale, u32 clamp, u32 out_reg) {
    u32 reg = s_tevc[stage];
    reg = set_field(reg, 1, 18, op & 1u);
    if (op <= 1) {
        reg = set_field(reg, 2, 20, scale);
        reg = set_field(reg, 2, 16, bias);
    } else {
        reg = set_field(reg, 2, 20, (op >> 1) & 3u);
        reg = set_field(reg, 2, 16, 3u);
    }
    reg = set_field(reg, 1, 19, clamp & 0xFFu);
    reg = set_field(reg, 2, 22, out_reg);
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
enum { GX_TEV_ADD = 0 };
enum { GX_TB_ZERO = 0 };
enum { GX_CS_SCALE_1 = 0 };
enum { GX_TRUE = 1 };
enum { GX_TEVPREV = 0 };

int main(void) {
    static u8 fifo[0x20000];
    GXInit(fifo, sizeof(fifo));

    GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);

    *(volatile u32 *)0x80300000 = 0xDEADBEEFu;
    *(volatile u32 *)0x80300004 = s_tevc[0];
    *(volatile u32 *)0x80300008 = s_bp_sent_not;
    while (1) {}
}
