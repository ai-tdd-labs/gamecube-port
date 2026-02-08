#include <stdint.h>

// Minimal, host-buildable slice of MP4 decomp: Hu3DInit from hsfman.c.
//
// Purpose: reachability/integration only (NOT a correctness oracle).
// This sets up only the state that later minimal Hu3D slices depend on.

typedef uint8_t u8;
typedef uint32_t u32;
typedef int16_t s16;

typedef struct {
    u8 r, g, b, a;
} GXColor;

typedef struct {
    void *hsfData;
    u32 attr;
} ModelDataMini;

enum { HU3D_MODEL_MAX = 512 };

extern ModelDataMini Hu3DData[HU3D_MODEL_MAX];
extern GXColor BGColor;

void Hu3DInit(void) {
    s16 i;
    for (i = 0; i < HU3D_MODEL_MAX; i++) {
        Hu3DData[i].hsfData = 0;
        Hu3DData[i].attr = 0;
    }
    BGColor.r = 0;
    BGColor.g = 0;
    BGColor.b = 0;
    BGColor.a = 0xFF;
}

