#include <stdint.h>

typedef uint32_t u32;
typedef uint16_t u16;

u32 gc_gx_color1x16_last;

void GXColor1x16(u16 index) {
    gc_gx_color1x16_last = (u32)index;
}
