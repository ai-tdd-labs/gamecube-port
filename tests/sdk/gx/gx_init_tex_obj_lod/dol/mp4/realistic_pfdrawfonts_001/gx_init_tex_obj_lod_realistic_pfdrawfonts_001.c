#include <stdint.h>
#include <string.h>

typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;
typedef float f32;

typedef struct {
    u32 mode0;
    u32 mode1;
    u32 image0;
    u32 image3;
    u32 userData;   // pointer slot, keep 0x20 layout
    u32 fmt;
    u32 tlutName;
    u16 loadCnt;
    u8 loadFmt;
    u8 flags;
} GXTexObj;

static inline u32 gx_some_set_reg_macro(u32 reg, u32 val) {
    // Mirror GXTexture.c SOME_SET_REG_MACRO (rlwinm(...,0,27,23) clears bits 24..26).
    return (reg & 0xF8FFFFFFu) | val;
}

static inline void gc_disable_interrupts(void) {
    u32 msr;
    __asm__ volatile("mfmsr %0" : "=r"(msr));
    msr &= ~0x8000u;
    __asm__ volatile("mtmsr %0; isync" : : "r"(msr));
}

static inline void gc_arm_decrementer_far_future(void) {
    __asm__ volatile(
        "lis 3,0x7fff\n"
        "ori 3,3,0xffff\n"
        "mtdec 3\n"
        :
        :
        : "r3");
}

static inline void gc_safepoint(void) {
    gc_disable_interrupts();
    gc_arm_decrementer_far_future();
}

static inline u32 set_field(u32 reg, u32 size, u32 shift, u32 v) {
    const u32 mask = ((1u << size) - 1u) << shift;
    return (reg & ~mask) | ((v << shift) & mask);
}

enum {
    GX_NEAR = 0,
    GX_LINEAR = 1,
    GX_ANISO_1 = 0,
};

static void GXInitTexObj(GXTexObj *obj, void *image_ptr, u16 width, u16 height, u32 format, u32 wrap_s, u32 wrap_t, u8 mipmap) {
    // Mirror decomp_mario_party_4/src/dolphin/gx/GXTexture.c:GXInitTexObj (observable packing only).
    (void)mipmap;
    memset(obj, 0, 0x20);
    obj->mode0 = set_field(obj->mode0, 2, 0, wrap_s);
    obj->mode0 = set_field(obj->mode0, 2, 2, wrap_t);
    obj->mode0 = set_field(obj->mode0, 1, 4, 1u);
    obj->mode0 = gx_some_set_reg_macro(obj->mode0, 0x80u);

    obj->fmt = format;
    obj->image0 = set_field(obj->image0, 10, 0, (u32)(width - 1u));
    obj->image0 = set_field(obj->image0, 10, 10, (u32)(height - 1u));
    obj->image0 = set_field(obj->image0, 4, 20, (u32)(format & 0xFu));

    {
        u32 imageBase = ((u32)(uintptr_t)image_ptr >> 5) & 0x01FFFFFFu;
        obj->image3 = set_field(obj->image3, 21, 0, imageBase);
    }

    // We don't need loadFmt/loadCnt/flags for this test; leave as zero.
}

static u8 GX2HWFiltConv[6] = { 0x00, 0x04, 0x01, 0x05, 0x02, 0x06 };

static void GXInitTexObjLOD(
    GXTexObj *obj,
    u32 min_filt,
    u32 mag_filt,
    f32 min_lod,
    f32 max_lod,
    f32 lod_bias,
    u8 bias_clamp,
    u8 do_edge_lod,
    u32 max_aniso
) {
    // Mirror observable packing from decomp_mario_party_4/src/dolphin/gx/GXTexture.c:GXInitTexObjLOD.
    u8 lbias;
    u8 lmin;
    u8 lmax;

    // clamp lod_bias to [-4, 3.99]
    if (lod_bias < -4.0f) lod_bias = -4.0f;
    else if (lod_bias >= 4.0f) lod_bias = 3.99f;
    lbias = (u8)(32.0f * lod_bias);
    obj->mode0 = set_field(obj->mode0, 8, 9, (u32)lbias);

    // mag/min filter
    obj->mode0 = set_field(obj->mode0, 1, 4, (mag_filt == GX_LINEAR) ? 1u : 0u);
    obj->mode0 = set_field(obj->mode0, 3, 5, (u32)GX2HWFiltConv[min_filt]);

    // do_edge_lod is inverted in mode0 bit8
    obj->mode0 = set_field(obj->mode0, 1, 8, do_edge_lod ? 0u : 1u);

    // fixed zeros + aniso + bias_clamp
    obj->mode0 = set_field(obj->mode0, 1, 17, 0u);
    obj->mode0 = set_field(obj->mode0, 1, 18, 0u);
    obj->mode0 = set_field(obj->mode0, 2, 19, (u32)max_aniso);
    obj->mode0 = set_field(obj->mode0, 1, 21, bias_clamp ? 1u : 0u);

    // clamp min/max lod to [0, 10] and pack as u8(16*x)
    if (min_lod < 0.0f) min_lod = 0.0f;
    else if (min_lod > 10.0f) min_lod = 10.0f;
    lmin = (u8)(16.0f * min_lod);

    if (max_lod < 0.0f) max_lod = 0.0f;
    else if (max_lod > 10.0f) max_lod = 10.0f;
    lmax = (u8)(16.0f * max_lod);

    obj->mode1 = set_field(obj->mode1, 8, 0, (u32)lmin);
    obj->mode1 = set_field(obj->mode1, 8, 8, (u32)lmax);
}

int main(void) {
    gc_safepoint();

    GXTexObj tex;
    GXInitTexObj(&tex, (void *)0x80010000u, 128, 128, 0, 0, 0, 0);

    // MP4 pfDrawFonts call
    GXInitTexObjLOD(&tex, GX_NEAR, GX_NEAR, 0.0f, 0.0f, 0.0f, 0, 0, GX_ANISO_1);

    *(volatile u32 *)0x80300000 = 0xDEADBEEFu;
    *(volatile u32 *)0x80300004 = tex.mode0;
    *(volatile u32 *)0x80300008 = tex.mode1;

    while (1) gc_safepoint();
}
