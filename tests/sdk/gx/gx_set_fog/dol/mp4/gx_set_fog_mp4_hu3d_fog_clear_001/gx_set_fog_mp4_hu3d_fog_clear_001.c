#include <stdint.h>
#include <string.h>

typedef uint32_t u32;
typedef uint8_t u8;
typedef float f32;

typedef struct { u8 r, g, b, a; } GXColor;

typedef enum {
    GX_FOG_NONE = 0,
} GXFogType;

static u32 s_last_ras_reg;
static u32 s_bp_sent_not;
static u32 s_fog0;
static u32 s_fog1;
static u32 s_fog2;
static u32 s_fog3;
static u32 s_fogclr;

static inline u32 set_field(u32 reg, u32 size, u32 shift, u32 v) {
    const u32 mask = ((1u << size) - 1u) << shift;
    return (reg & ~mask) | ((v << shift) & mask);
}

static void GX_WRITE_RAS_REG(u32 v) { s_last_ras_reg = v; }

static inline void gc_disable_interrupts(void) {
    u32 msr;
    __asm__ volatile("mfmsr %0" : "=r"(msr));
    msr &= ~0x8000u; // MSR[EE]
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

static void GXSetFog(GXFogType type, f32 startz, f32 endz, f32 nearz, f32 farz, GXColor color) {
    // Mirror decomp_mario_party_4/src/dolphin/gx/GXPixel.c:GXSetFog
    u32 fogclr, fog0, fog1, fog2, fog3;
    f32 A, B, B_mant, C;
    f32 a, c;
    u32 B_expn;
    u32 b_m;
    u32 b_s;
    u32 a_hex;
    u32 c_hex;

    if (farz == nearz || endz == startz) {
        A = 0.0f;
        B = 0.5f;
        C = 0.0f;
    } else {
        A = (farz * nearz) / ((farz - nearz) * (endz - startz));
        B = farz / (farz - nearz);
        C = startz / (endz - startz);
    }

    B_mant = B;
    B_expn = 0;
    while (B_mant > 1.0f) {
        B_mant *= 0.5f;
        B_expn++;
    }
    while (B_mant > 0.0f && B_mant < 0.5f) {
        B_mant *= 2.0f;
        B_expn--;
    }

    a = A / (f32)(1u << (B_expn + 1u));
    b_m = (u32)(8.388638e6f * B_mant);
    b_s = B_expn + 1u;
    c = C;

    fog1 = 0;
    fog1 = set_field(fog1, 24, 0, b_m);
    fog1 = set_field(fog1, 8, 24, 0xEFu);

    fog2 = 0;
    fog2 = set_field(fog2, 5, 0, b_s);
    fog2 = set_field(fog2, 8, 24, 0xF0u);

    memcpy(&a_hex, &a, sizeof(a_hex));
    memcpy(&c_hex, &c, sizeof(c_hex));

    fog0 = 0;
    fog0 = set_field(fog0, 11, 0, (a_hex >> 12) & 0x7FFu);
    fog0 = set_field(fog0, 8, 11, (a_hex >> 23) & 0xFFu);
    fog0 = set_field(fog0, 1, 19, (a_hex >> 31));
    fog0 = set_field(fog0, 8, 24, 0xEEu);

    fog3 = 0;
    fog3 = set_field(fog3, 11, 0, (c_hex >> 12) & 0x7FFu);
    fog3 = set_field(fog3, 8, 11, (c_hex >> 23) & 0xFFu);
    fog3 = set_field(fog3, 1, 19, (c_hex >> 31));
    fog3 = set_field(fog3, 1, 20, 0);
    fog3 = set_field(fog3, 3, 21, (u32)type);
    fog3 = set_field(fog3, 8, 24, 0xF1u);

    fogclr = 0;
    fogclr = set_field(fogclr, 8, 0, color.b);
    fogclr = set_field(fogclr, 8, 8, color.g);
    fogclr = set_field(fogclr, 8, 16, color.r);
    fogclr = set_field(fogclr, 8, 24, 0xF2u);

    s_fog0 = fog0;
    s_fog1 = fog1;
    s_fog2 = fog2;
    s_fog3 = fog3;
    s_fogclr = fogclr;

    GX_WRITE_RAS_REG(fog0);
    GX_WRITE_RAS_REG(fog1);
    GX_WRITE_RAS_REG(fog2);
    GX_WRITE_RAS_REG(fog3);
    GX_WRITE_RAS_REG(fogclr);
    s_bp_sent_not = 0;
}

int main(void) {
    gc_safepoint();

    // MP4 Hu3DFogClear: GXSetFog(GX_FOG_NONE, 0,0,0,0, BGColor)
    GXColor bg;
    bg.r = 0;
    bg.g = 0;
    bg.b = 0;
    bg.a = 0xFF;

    s_last_ras_reg = 0;
    s_bp_sent_not = 1;
    s_fog0 = s_fog1 = s_fog2 = s_fog3 = s_fogclr = 0;

    GXSetFog(GX_FOG_NONE, 0.0f, 0.0f, 0.0f, 0.0f, bg);

    *(volatile u32*)0x80300000 = 0xDEADBEEFu;
    *(volatile u32*)0x80300004 = s_fog0;
    *(volatile u32*)0x80300008 = s_fog1;
    *(volatile u32*)0x8030000C = s_fog2;
    *(volatile u32*)0x80300010 = s_fog3;
    *(volatile u32*)0x80300014 = s_fogclr;
    *(volatile u32*)0x80300018 = s_last_ras_reg;
    *(volatile u32*)0x8030001C = s_bp_sent_not;

    while (1) { gc_safepoint(); }
}
