#include <stdint.h>

typedef uint32_t u32;
typedef uint8_t u8;

typedef struct { u8 r, g, b, a; } GXColor;

static u32 s_last_ras_reg;
static u32 s_bp_sent_not;
static u32 s_reg0;
static u32 s_reg1;
static u32 s_reg2;

// Prevent decrementer/interrupt exceptions (Dolphin may otherwise trap to 0x900
// before any handlers are installed in these tiny DOLs).
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

static void GX_WRITE_RAS_REG(u32 v) {
    s_last_ras_reg = v;
}

static void GXSetCopyClear(GXColor clear_clr, u32 clear_z) {
    // Mirror decomp_mario_party_4/src/dolphin/gx/GXFrameBuf.c:GXSetCopyClear
    u32 reg;

    reg = 0;
    reg = set_field(reg, 8, 0, (u32)clear_clr.r);
    reg = set_field(reg, 8, 8, (u32)clear_clr.a);
    reg = set_field(reg, 8, 24, 0x4Fu);
    s_reg0 = reg;
    GX_WRITE_RAS_REG(reg);

    reg = 0;
    reg = set_field(reg, 8, 0, (u32)clear_clr.b);
    reg = set_field(reg, 8, 8, (u32)clear_clr.g);
    reg = set_field(reg, 8, 24, 0x50u);
    s_reg1 = reg;
    GX_WRITE_RAS_REG(reg);

    reg = 0;
    reg = set_field(reg, 24, 0, clear_z & 0xFFFFFFu);
    reg = set_field(reg, 8, 24, 0x51u);
    s_reg2 = reg;
    GX_WRITE_RAS_REG(reg);

    s_bp_sent_not = 0;
}

int main(void) {
    gc_safepoint();

    // MP4 Hu3DPreProc: GXSetCopyClear(BGColor, 0xFFFFFF)
    GXColor bg;
    bg.r = 0;
    bg.g = 0;
    bg.b = 0;
    bg.a = 0xFF;

    s_last_ras_reg = 0;
    s_bp_sent_not = 1;
    s_reg0 = s_reg1 = s_reg2 = 0;

    GXSetCopyClear(bg, 0x00FFFFFFu);

    *(volatile u32*)0x80300000 = 0xDEADBEEFu;
    *(volatile u32*)0x80300004 = s_reg0;
    *(volatile u32*)0x80300008 = s_reg1;
    *(volatile u32*)0x8030000C = s_reg2;
    *(volatile u32*)0x80300010 = s_last_ras_reg;
    *(volatile u32*)0x80300014 = s_bp_sent_not;
    while (1) { gc_safepoint(); }
}
