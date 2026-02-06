#include <stdint.h>
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;

typedef struct { u8 _dummy; } GXFifoObj;
static GXFifoObj s_fifo_obj;

static u32 s_cp_disp_stride;

static inline u32 set_field(u32 reg, u32 size, u32 shift, u32 v) {
    const u32 mask = ((1u << size) - 1u) << shift;
    return (reg & ~mask) | ((v << shift) & mask);
}

static GXFifoObj *GXInit(void *base, u32 size) {
    (void)base; (void)size;
    s_cp_disp_stride = 0;
    return &s_fifo_obj;
}

static void GXSetDispCopyDst(u16 wd, u16 ht) {
    (void)ht;
    u16 stride = (u16)(wd * 2u);
    u32 reg = 0;
    reg = set_field(reg, 10, 0, (u32)(stride >> 5));
    reg = set_field(reg, 8, 24, 0x4D);
    s_cp_disp_stride = reg;
}

int main(void) {
    static u8 fifo[0x20000];
    GXInit(fifo, sizeof(fifo));
    GXSetDispCopyDst(640, 464);

    *(volatile u32 *)0x80300000 = 0xDEADBEEFu;
    *(volatile u32 *)0x80300004 = s_cp_disp_stride;
    while (1) {}
}
