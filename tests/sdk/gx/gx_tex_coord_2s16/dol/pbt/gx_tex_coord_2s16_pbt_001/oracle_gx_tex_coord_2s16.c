#include <stdint.h>

typedef uint32_t u32;
typedef int32_t s32;
typedef int16_t s16;

u32 gc_gx_texcoord2s16_s;
u32 gc_gx_texcoord2s16_t;

void GXTexCoord2s16(s16 s, s16 t) {
    gc_gx_texcoord2s16_s = (u32)(s32)s;
    gc_gx_texcoord2s16_t = (u32)(s32)t;
}
