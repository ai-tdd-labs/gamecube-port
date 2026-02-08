#include <stdint.h>

typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;

typedef struct {
    u8 r, g, b, a;
} GXColor;

static u32 s_reg_ra;
static u32 s_reg_bg;
static u32 s_last_ras;

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

static inline u32 set_field(u32 reg, u32 size, u32 shift, u32 v) {
    const u32 mask = ((1u << size) - 1u) << shift;
    return (reg & ~mask) | ((v << shift) & mask);
}

static void GXSetTevColor(u32 id, GXColor color) {
    // Mirror decomp_mario_party_4/src/dolphin/gx/GXTev.c:GXSetTevColor.
    u32 regRA = 0;
    u32 regBG = 0;
    regRA = set_field(regRA, 11, 0, (u32)color.r);
    regRA = set_field(regRA, 11, 12, (u32)color.a);
    regRA = set_field(regRA, 8, 24, (u32)(224u + id * 2u));

    regBG = set_field(regBG, 11, 0, (u32)color.b);
    regBG = set_field(regBG, 11, 12, (u32)color.g);
    regBG = set_field(regBG, 8, 24, (u32)(225u + id * 2u));

    // Observable values for our deterministic model.
    s_reg_ra = regRA;
    s_reg_bg = regBG;
    s_last_ras = regBG; // final write in the SDK sequence
}

int main(void) {
    gc_safepoint();

    // MP4 wipe uses GXSetTevColor(GX_TEVSTAGE1, color) where numeric 1 maps to GX_TEVREG0 (C0).
    GXColor c = { 0x12, 0x34, 0x56, 0x78 };
    GXSetTevColor(1, c);

    *(volatile u32 *)0x80300000 = 0xDEADBEEFu;
    *(volatile u32 *)0x80300004 = s_reg_ra;
    *(volatile u32 *)0x80300008 = s_reg_bg;
    *(volatile u32 *)0x8030000C = s_last_ras;
    {
        u32 off;
        for (off = 0x10; off < 0x40; off += 4) {
            *(volatile u32 *)(0x80300000u + off) = 0;
        }
    }

    while (1) gc_safepoint();
}

