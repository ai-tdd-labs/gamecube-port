#include <stdint.h>

typedef uint32_t u32;
typedef uint8_t u8;

typedef struct { u8 _dummy; } GXFifoObj;
static GXFifoObj s_fifo_obj;

static u32 s_last_c;

static void GXColor1x8(u8 c) { s_last_c = (u32)c; }

static GXFifoObj *GXInit(void *base, u32 size) {
    (void)base;
    (void)size;
    s_last_c = 0;
    return &s_fifo_obj;
}

int main(void) {
    static u8 fifo[0x20000];
    GXInit(fifo, sizeof(fifo));

    // MP4 pfDrawFonts passes a single 8-bit color (used repeatedly per glyph).
    GXColor1x8((u8)0x7Fu);

    *(volatile u32 *)0x80300000 = 0xDEADBEEFu;
    *(volatile u32 *)0x80300004 = s_last_c;
    while (1) {}
}

