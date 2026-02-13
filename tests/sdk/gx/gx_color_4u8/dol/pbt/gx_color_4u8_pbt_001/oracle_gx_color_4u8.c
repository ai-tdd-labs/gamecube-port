#include <stdint.h>

typedef uint32_t u32;
typedef uint8_t u8;

u32 gc_gx_color4u8_last;

void GXColor4u8(u8 r, u8 g, u8 b, u8 a) {
    gc_gx_color4u8_last = ((u32)r << 24) | ((u32)g << 16) | ((u32)b << 8) | (u32)a;
}
