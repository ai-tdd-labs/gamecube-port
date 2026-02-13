#include <stdint.h>

typedef uint32_t u32;
typedef int16_t s16;

typedef struct { s16 r, g, b, a; } GXColorS10;

u32 gc_gx_tev_colors10_ra_last;
u32 gc_gx_tev_colors10_bg_last;
u32 gc_gx_bp_sent_not;

static inline u32 set_field(u32 reg, u32 width, u32 shift, u32 val) {
    u32 mask;
    if (width >= 32u) {
        mask = 0xFFFFFFFFu;
    } else {
        mask = ((1u << width) - 1u) << shift;
    }
    return (reg & ~mask) | ((val << shift) & mask);
}

void GXSetTevColorS10(u32 id, GXColorS10 color) {
    u32 regRA = 0u;
    regRA = set_field(regRA, 11u, 0u, (u32)(color.r & 0x7FF));
    regRA = set_field(regRA, 11u, 12u, (u32)(color.a & 0x7FF));
    regRA = set_field(regRA, 8u, 24u, 224u + id * 2u);

    u32 regBG = 0u;
    regBG = set_field(regBG, 11u, 0u, (u32)(color.b & 0x7FF));
    regBG = set_field(regBG, 11u, 12u, (u32)(color.g & 0x7FF));
    regBG = set_field(regBG, 8u, 24u, 225u + id * 2u);

    gc_gx_tev_colors10_ra_last = regRA;
    gc_gx_tev_colors10_bg_last = regBG;
    gc_gx_bp_sent_not = 0u;
}
