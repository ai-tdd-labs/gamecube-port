#include <stdint.h>

typedef uint32_t u32;
typedef uint8_t u8;

typedef struct { u8 _dummy; } GXFifoObj;
static GXFifoObj s_fifo_obj;

static u32 s_x;
static u32 s_y;
static u32 s_z;

static inline u32 f32_bits(float f) {
    union { float f; u32 u; } u;
    u.f = f;
    return u.u;
}

static void GXPosition3f32(float x, float y, float z) {
    s_x = f32_bits(x);
    s_y = f32_bits(y);
    s_z = f32_bits(z);
}

static GXFifoObj *GXInit(void *base, u32 size) {
    (void)base; (void)size;
    s_x = s_y = s_z = 0;
    return &s_fifo_obj;
}

int main(void) {
    static u8 fifo[0x20000];
    GXInit(fifo, sizeof(fifo));

    GXPosition3f32(1.25f, -2.5f, 3.75f);

    *(volatile u32 *)0x80300000 = 0xDEADBEEFu;
    *(volatile u32 *)0x80300004 = s_x;
    *(volatile u32 *)0x80300008 = s_y;
    *(volatile u32 *)0x8030000C = s_z;
    while (1) {}
}
