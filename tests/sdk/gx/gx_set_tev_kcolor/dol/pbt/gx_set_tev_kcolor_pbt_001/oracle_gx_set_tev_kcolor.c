#include <stdint.h>

typedef uint32_t u32;
typedef uint8_t u8;

typedef struct { u8 r, g, b, a; } GXColor;

u32 gc_gx_tev_ksel[8];
u32 gc_gx_tev_kcolor_ra[4];
u32 gc_gx_tev_kcolor_bg[4];
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

void GXSetTevKColor(u32 id, GXColor color) {
    u32 regRA = 0u;
    regRA = set_field(regRA, 8u, 0u, (u32)color.r);
    regRA = set_field(regRA, 8u, 12u, (u32)color.a);
    regRA = set_field(regRA, 4u, 20u, 8u);
    regRA = set_field(regRA, 8u, 24u, 224u + id * 2u);

    u32 regBG = 0u;
    regBG = set_field(regBG, 8u, 0u, (u32)color.b);
    regBG = set_field(regBG, 8u, 12u, (u32)color.g);
    regBG = set_field(regBG, 4u, 20u, 8u);
    regBG = set_field(regBG, 8u, 24u, 225u + id * 2u);

    if (id < 4u) {
        gc_gx_tev_kcolor_ra[id] = regRA;
        gc_gx_tev_kcolor_bg[id] = regBG;
    }

    gc_gx_bp_sent_not = 0u;
}

void GXSetTevKColorSel(u32 stage, u32 sel) {
    if (stage >= 16u) return;
    u32 idx = stage >> 1;
    if (stage & 1u) {
        gc_gx_tev_ksel[idx] = set_field(gc_gx_tev_ksel[idx], 5u, 14u, sel);
    } else {
        gc_gx_tev_ksel[idx] = set_field(gc_gx_tev_ksel[idx], 5u, 4u, sel);
    }
    gc_gx_bp_sent_not = 0u;
}

void GXSetTevKAlphaSel(u32 stage, u32 sel) {
    if (stage >= 16u) return;
    u32 idx = stage >> 1;
    if (stage & 1u) {
        gc_gx_tev_ksel[idx] = set_field(gc_gx_tev_ksel[idx], 5u, 19u, sel);
    } else {
        gc_gx_tev_ksel[idx] = set_field(gc_gx_tev_ksel[idx], 5u, 9u, sel);
    }
    gc_gx_bp_sent_not = 0u;
}
