#include <stdint.h>

typedef uint32_t u32;
typedef uint8_t u8;

typedef struct { u8 _dummy; } GXFifoObj;
static GXFifoObj s_fifo_obj;

static u32 s_s;
static u32 s_t;

static inline u32 f32_bits(float f) {
    union { float f; u32 u; } u;
    u.f = f;
    return u.u;
}

static void GXTexCoord2f32(float s, float t) {
    s_s = f32_bits(s);
    s_t = f32_bits(t);
}

static GXFifoObj *GXInit(void *base, u32 size) {
    (void)base;
    (void)size;
    s_s = s_t = 0;
    return &s_fifo_obj;
}

int main(void) {
    static u8 fifo[0x20000];
    GXInit(fifo, sizeof(fifo));

    // MP4 pfDrawFonts uses normalized atlas texcoords.
    GXTexCoord2f32(0.25f, 0.5f);

    *(volatile u32 *)0x80300000 = 0xDEADBEEFu;
    *(volatile u32 *)0x80300004 = s_s;
    *(volatile u32 *)0x80300008 = s_t;
    while (1) {}
}

