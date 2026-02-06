#include <stdint.h>
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;

typedef struct { u8 _dummy; } GXFifoObj;
static GXFifoObj s_fifo_obj;

static u32 s_cp_disp_src;
static u32 s_cp_disp_size;

static inline u32 set_field(u32 reg, u32 size, u32 shift, u32 v) {
    const u32 mask = ((1u << size) - 1u) << shift;
    return (reg & ~mask) | ((v << shift) & mask);
}

static GXFifoObj *GXInit(void *base, u32 size) {
    (void)base; (void)size;
    s_cp_disp_src = 0;
    s_cp_disp_size = 0;
    return &s_fifo_obj;
}

static void GXSetDispCopySrc(u16 left, u16 top, u16 wd, u16 ht) {
    u32 reg = 0;
    reg = set_field(reg, 10, 0, left);
    reg = set_field(reg, 10, 10, top);
    reg = set_field(reg, 8, 24, 0x49);
    s_cp_disp_src = reg;

    reg = 0;
    reg = set_field(reg, 10, 0, (u32)(wd - 1u));
    reg = set_field(reg, 10, 10, (u32)(ht - 1u));
    reg = set_field(reg, 8, 24, 0x4A);
    s_cp_disp_size = reg;
}

int main(void) {
    static u8 fifo[0x20000];
    GXInit(fifo, sizeof(fifo));
    GXSetDispCopySrc(0, 0, 640, 448);

    *(volatile u32 *)0x80300000 = 0xDEADBEEFu;
    *(volatile u32 *)0x80300004 = s_cp_disp_src;
    *(volatile u32 *)0x80300008 = s_cp_disp_size;
    while (1) {}
}
