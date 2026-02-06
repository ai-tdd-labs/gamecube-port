#include <stdint.h>

typedef uint32_t u32;
typedef uint8_t u8;

typedef struct { u8 _dummy; } GXFifoObj;
static GXFifoObj s_fifo_obj;

static u32 s_gen_mode;
static u32 s_dirty_state;

static inline u32 set_field(u32 reg, u32 size, u32 shift, u32 v) {
    const u32 mask = ((1u << size) - 1u) << shift;
    return (reg & ~mask) | ((v << shift) & mask);
}

static GXFifoObj *GXInit(void *base, u32 size) {
    (void)base; (void)size;
    s_gen_mode = 0;
    s_dirty_state = 0;
    return &s_fifo_obj;
}

static void GXSetCullMode(u32 mode) {
    u32 hwMode = mode;
    if (mode == 1u) hwMode = 2u;
    else if (mode == 2u) hwMode = 1u;
    s_gen_mode = set_field(s_gen_mode, 2, 14, hwMode & 3u);
    s_dirty_state |= 4u;
}

enum { GX_CULL_BACK = 2 };

int main(void) {
    static u8 fifo[0x20000];
    GXInit(fifo, sizeof(fifo));

    GXSetCullMode(GX_CULL_BACK);

    *(volatile u32 *)0x80300000 = 0xDEADBEEFu;
    *(volatile u32 *)0x80300004 = s_gen_mode;
    *(volatile u32 *)0x80300008 = s_dirty_state;
    while (1) {}
}
