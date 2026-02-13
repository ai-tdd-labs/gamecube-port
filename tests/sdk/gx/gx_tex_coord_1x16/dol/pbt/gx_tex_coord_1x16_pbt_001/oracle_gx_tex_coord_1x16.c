#include <stdint.h>

typedef uint32_t u32;
typedef uint16_t u16;

u32 gc_gx_texcoord1x16_last;

void GXTexCoord1x16(u16 index) {
    gc_gx_texcoord1x16_last = (u32)index;
}
