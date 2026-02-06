#include <stdint.h>
typedef uint32_t u32;
typedef uint16_t u16;

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

static float GXGetYScaleFactor(u16 efbHeight, u16 xfbHeight) {
    u32 tgtHt = xfbHeight;
    float yScale = (float)xfbHeight / (float)efbHeight;
    u32 iScale = ((u32)(256.0f / yScale)) & 0x1FFu;
    u32 realHt = __GXGetNumXfbLines(efbHeight, iScale);

    while (realHt > xfbHeight) {
        tgtHt--;
        yScale = (float)tgtHt / (float)efbHeight;
        iScale = ((u32)(256.0f / yScale)) & 0x1FFu;
        realHt = __GXGetNumXfbLines(efbHeight, iScale);
    }

    float fScale = yScale;
    while (realHt < xfbHeight) {
        fScale = yScale;
        tgtHt++;
        yScale = (float)tgtHt / (float)efbHeight;
        iScale = ((u32)(256.0f / yScale)) & 0x1FFu;
        realHt = __GXGetNumXfbLines(efbHeight, iScale);
    }

    return fScale;
}

int main(void) {
    // MP4 init.c PAL path calls:
    //   GXGetYScaleFactor(RenderMode->efbHeight, RenderMode->xfbHeight)
    // On PAL, efbHeight is patched to 480 while xfbHeight is typically 528.
    float f = GXGetYScaleFactor(480, 528);
    union { float f; u32 u; } u = { .f = f };

    *(volatile u32 *)0x80300000 = 0xDEADBEEFu;
    *(volatile u32 *)0x80300004 = u.u;
    while (1) {}
}
