#include <stdint.h>

typedef uint32_t u32;
typedef uint16_t u16;

static u32 s_cp_tex_src;
static u32 s_cp_tex_size;

static inline u32 set_field(u32 reg, u32 size, u32 shift, u32 v) {
    const u32 mask = ((1u << size) - 1u) << shift;
    return (reg & ~mask) | ((v << shift) & mask);
}

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

static void GXSetTexCopySrc(u16 left, u16 top, u16 wd, u16 ht) {
    // Mirror GXFrameBuf.c:GXSetTexCopySrc observable packing.
    s_cp_tex_src = 0;
    s_cp_tex_src = set_field(s_cp_tex_src, 10, 0, (u32)left);
    s_cp_tex_src = set_field(s_cp_tex_src, 10, 10, (u32)top);
    s_cp_tex_src = set_field(s_cp_tex_src, 8, 24, 0x49u);

    s_cp_tex_size = 0;
    s_cp_tex_size = set_field(s_cp_tex_size, 10, 0, (u32)(wd - 1u));
    s_cp_tex_size = set_field(s_cp_tex_size, 10, 10, (u32)(ht - 1u));
    s_cp_tex_size = set_field(s_cp_tex_size, 8, 24, 0x4Au);
}

int main(void) {
    gc_safepoint();

    // MP4 Hu3DShadowExec small branch (unk_02 <= 0xF0).
    // Use unk_02=0xF0, so wd=ht=0x1E0.
    const u16 unk_02 = 0x00F0;
    GXSetTexCopySrc(0, 0, (u16)(unk_02 * 2), (u16)(unk_02 * 2));

    *(volatile u32 *)0x80300000 = 0xDEADBEEFu;
    *(volatile u32 *)0x80300004 = s_cp_tex_src;
    *(volatile u32 *)0x80300008 = s_cp_tex_size;
    *(volatile u32 *)0x8030000C = 0;

    while (1) gc_safepoint();
}

