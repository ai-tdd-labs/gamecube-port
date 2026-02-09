#include <stdint.h>

typedef uint32_t u32;
typedef uint8_t u8;

typedef struct { u8 _dummy; } GXFifoObj;
static GXFifoObj s_fifo_obj;

static u32 s_last_rgb;

static void GXColor3u8(u8 r, u8 g, u8 b) {
    s_last_rgb = ((u32)r << 16) | ((u32)g << 8) | (u32)b;
}

static GXFifoObj *GXInit(void *base, u32 size) {
    (void)base;
    (void)size;
    s_last_rgb = 0;
    return &s_fifo_obj;
}

int main(void) {
    static u8 fifo[0x20000];
    GXInit(fifo, sizeof(fifo));

    // MP4 pfDrawFonts uses GXColor3u8 for debug/safety geometry.
    GXColor3u8((u8)255, (u8)0, (u8)0);

    *(volatile u32 *)0x80300000 = 0xDEADBEEFu;
    *(volatile u32 *)0x80300004 = s_last_rgb;
    while (1) {}
}

