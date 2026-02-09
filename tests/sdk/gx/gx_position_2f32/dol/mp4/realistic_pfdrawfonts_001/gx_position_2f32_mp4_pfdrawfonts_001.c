#include <stdint.h>

typedef uint32_t u32;
typedef uint8_t u8;

typedef struct { u8 _dummy; } GXFifoObj;
static GXFifoObj s_fifo_obj;

static u32 s_x;
static u32 s_y;

static inline u32 f32_bits(float f) {
    union { float f; u32 u; } u;
    u.f = f;
    return u.u;
}

static void GXPosition2f32(float x, float y) {
    s_x = f32_bits(x);
    s_y = f32_bits(y);
}

static GXFifoObj *GXInit(void *base, u32 size) {
    (void)base;
    (void)size;
    s_x = s_y = 0;
    return &s_fifo_obj;
}

int main(void) {
    static u8 fifo[0x20000];
    GXInit(fifo, sizeof(fifo));

    // MP4 pfDrawFonts uses f32 screen-space verts for debug/safety geometry.
    GXPosition2f32(64.0f, 48.0f);

    *(volatile u32 *)0x80300000 = 0xDEADBEEFu;
    *(volatile u32 *)0x80300004 = s_x;
    *(volatile u32 *)0x80300008 = s_y;
    while (1) {}
}

