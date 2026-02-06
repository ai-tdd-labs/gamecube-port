#include <stdint.h>
typedef uint32_t u32;

typedef struct { uint8_t _dummy; } GXFifoObj;

static volatile float s_top;

static void GXSetViewportJitter(float left, float top, float wd, float ht, float nearz, float farz, u32 field) {
    (void)left; (void)wd; (void)ht; (void)nearz; (void)farz;
    if (field == 0) top -= 0.5f;
    s_top = top;
}

int main(void) {
    // MP4 HuSysBeforeRender uses GXSetViewportJitter(..., VIGetNextField()) when field_rendering is set.
    GXSetViewportJitter(0, 0, 640, 480, 0, 1, 0);

    union { float f; u32 u; } u = { .f = s_top };
    *(volatile u32*)0x80300000 = 0xDEADBEEFu;
    *(volatile u32*)0x80300004 = u.u;
    while (1) {}
}
