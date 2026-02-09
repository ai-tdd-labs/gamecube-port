#include <stdint.h>

typedef uint32_t u32;
typedef uint8_t u8;
typedef int16_t s16;

typedef struct { u8 _dummy; } GXFifoObj;
static GXFifoObj s_fifo_obj;

static u32 s_x;
static u32 s_y;
static u32 s_z;

static void GXPosition3s16(s16 x, s16 y, s16 z) {
    s_x = (u32)(signed int)x;
    s_y = (u32)(signed int)y;
    s_z = (u32)(signed int)z;
}

static GXFifoObj *GXInit(void *base, u32 size) {
    (void)base;
    (void)size;
    s_x = s_y = s_z = 0;
    return &s_fifo_obj;
}

int main(void) {
    static u8 fifo[0x20000];
    GXInit(fifo, sizeof(fifo));

    // MP4 pfDrawFonts uses s16 screen-space verts for glyph quads.
    GXPosition3s16((s16)123, (s16)-456, (s16)0);

    *(volatile u32 *)0x80300000 = 0xDEADBEEFu;
    *(volatile u32 *)0x80300004 = s_x;
    *(volatile u32 *)0x80300008 = s_y;
    *(volatile u32 *)0x8030000C = s_z;
    while (1) {}
}

