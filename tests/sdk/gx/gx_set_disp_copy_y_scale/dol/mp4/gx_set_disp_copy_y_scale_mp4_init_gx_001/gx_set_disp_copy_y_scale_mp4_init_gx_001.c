#include <stdint.h>
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;

typedef struct { u8 _dummy; } GXFifoObj;
static GXFifoObj s_fifo_obj;

static u32 s_cp_disp_size;
static u32 s_cp_disp;

static inline u32 set_field(u32 reg, u32 size, u32 shift, u32 v) {
    const u32 mask = ((1u << size) - 1u) << shift;
    return (reg & ~mask) | ((v << shift) & mask);
}

static u32 __GXGetNumXfbLines(u32 efbHt, u32 iScale) {
    u32 count = (efbHt - 1u) * 0x100u;
    u32 realHt = (count / iScale) + 1u;
    u32 iScaleD = iScale;

    if (iScaleD > 0x80u && iScaleD < 0x100u) {
        while ((iScaleD % 2u) == 0u) iScaleD /= 2u;
        if ((efbHt % iScaleD) == 0u) realHt++;
    }
    if (realHt > 0x400u) realHt = 0x400u;
    return realHt;
}

static GXFifoObj *GXInit(void *base, u32 size) {
    (void)base; (void)size;
    s_cp_disp_size = 0;
    s_cp_disp = 0;
    return &s_fifo_obj;
}

static void GXSetDispCopySrc(u16 left, u16 top, u16 wd, u16 ht) {
    (void)left; (void)top; (void)wd;
    u32 reg = 0;
    reg = set_field(reg, 10, 10, (u32)(ht - 1u));
    s_cp_disp_size = reg;
}

static u32 GXSetDispCopyYScale(float vscale) {
    u32 scale = ((u32)(256.0f / vscale)) & 0x1FFu;
    u32 check = (scale != 0x100u);

    s_cp_disp = set_field(s_cp_disp, 1, 10, check);

    u32 height = ((s_cp_disp_size >> 10) & 0x3FFu) + 1u;
    return __GXGetNumXfbLines(height, scale);
}

int main(void) {
    static u8 fifo[0x20000];
    GXInit(fifo, sizeof(fifo));
    GXSetDispCopySrc(0, 0, 640, 448);
    u32 lines = GXSetDispCopyYScale(1.0f);

    *(volatile u32 *)0x80300000 = 0xDEADBEEFu;
    *(volatile u32 *)0x80300004 = lines;
    *(volatile u32 *)0x80300008 = s_cp_disp;
    while (1) {}
}
