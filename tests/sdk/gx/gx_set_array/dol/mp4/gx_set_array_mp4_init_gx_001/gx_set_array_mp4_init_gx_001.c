#include <stdint.h>

typedef uint32_t u32;
typedef uint8_t u8;

typedef struct { u8 _dummy; } GXFifoObj;
static GXFifoObj s_fifo_obj;

static u32 s_base[32];
static u32 s_stride[32];

static void GXSetArray(u32 attr, const void *base_ptr, u8 stride) {
    // SDK: NBT aliases to NRM.
    if (attr == 25u) attr = 10u;
    if (attr < 9u) return;
    u32 cp_attr = attr - 9u;
    u32 phy_addr = (u32)(uintptr_t)base_ptr & 0x3FFFFFFFu;
    s_base[cp_attr] = phy_addr;
    s_stride[cp_attr] = (u32)stride;
}

static GXFifoObj *GXInit(void *base, u32 size) {
    (void)base; (void)size;
    u32 i;
    for (i = 0; i < 32; i++) { s_base[i] = 0; s_stride[i] = 0; }
    return &s_fifo_obj;
}

enum { GX_VA_POS = 9 };

int main(void) {
    static u8 fifo[0x20000];
    GXInit(fifo, sizeof(fifo));

    GXSetArray(GX_VA_POS, (const void*)0x81234567u, 0x20);

    *(volatile u32 *)0x80300000 = 0xDEADBEEFu;
    *(volatile u32 *)0x80300004 = s_base[0];
    *(volatile u32 *)0x80300008 = s_stride[0];
    while (1) {}
}
