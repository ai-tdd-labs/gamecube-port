#include <stdint.h>

typedef uint32_t u32;
typedef int32_t s32;
typedef int16_t s16;

u32 gc_gx_normal3s16_x;
u32 gc_gx_normal3s16_y;
u32 gc_gx_normal3s16_z;

void GXNormal3s16(s16 x, s16 y, s16 z) {
    gc_gx_normal3s16_x = (u32)(s32)x;
    gc_gx_normal3s16_y = (u32)(s32)y;
    gc_gx_normal3s16_z = (u32)(s32)z;
}
