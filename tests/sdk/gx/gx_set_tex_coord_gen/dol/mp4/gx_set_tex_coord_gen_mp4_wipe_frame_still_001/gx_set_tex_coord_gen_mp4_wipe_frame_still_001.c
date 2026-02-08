#include <stdint.h>

typedef uint32_t u32;
typedef uint8_t u8;

static u32 s_xf40;
static u32 s_xf50;
static u32 s_mat_idx_a;
static u32 s_xf24;

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

// Minimal constants (from GXEnum.h) for the MP4 wipe callsite.
enum {
    GX_TEXCOORD0 = 0,

    GX_TG_MTX3x4 = 0,
    GX_TG_MTX2x4 = 1,

    GX_TG_TEX0 = 4,

    GX_IDENTITY = 60,
    GX_PTIDENTITY = 125,
};

static void __GXSetMatrixIndex_Tex0(void) {
    // For TEXCOORD0, matIdxAttr = dst_coord + 1, which is < GX_VA_TEX4MTXIDX.
    // The observable side effect we validate is XF reg 24 = matIdxA.
    s_xf24 = s_mat_idx_a;
}

static void GXSetTexCoordGen2(u8 dst_coord, u8 func, u8 src_param, u32 mtx, u8 normalize, u32 postmtx) {
    // Mirror decomp_mario_party_4/src/dolphin/gx/GXAttr.c:GXSetTexCoordGen2.
    u32 reg = 0;
    u32 row = 5;
    u32 form = 0;

    // Only the MP4 wipe case is modeled here: src_param == GX_TG_TEX0.
    if (src_param == GX_TG_TEX0) {
        row = 5;
        form = 0;
    }

    if (func == GX_TG_MTX2x4) {
        reg = set_field(reg, 1, 1, 0);
        reg = set_field(reg, 1, 2, form);
        reg = set_field(reg, 3, 4, 0);
        reg = set_field(reg, 5, 7, row);
    } else if (func == GX_TG_MTX3x4) {
        reg = set_field(reg, 1, 1, 1);
        reg = set_field(reg, 1, 2, form);
        reg = set_field(reg, 3, 4, 0);
        reg = set_field(reg, 5, 7, row);
    }

    // XF reg 0x40 + dst_coord.
    if (dst_coord == GX_TEXCOORD0) {
        s_xf40 = reg;
    }

    // XF reg 0x50 + dst_coord.
    reg = 0;
    reg = set_field(reg, 6, 0, (u32)(postmtx - 64u));
    reg = set_field(reg, 1, 8, (u32)(normalize != 0));
    if (dst_coord == GX_TEXCOORD0) {
        s_xf50 = reg;
    }

    // matIdxA update for TEXCOORD0 stores 6-bit mtx at bits [6..11].
    if (dst_coord == GX_TEXCOORD0) {
        s_mat_idx_a = set_field(s_mat_idx_a, 6, 6, mtx & 0x3Fu);
        __GXSetMatrixIndex_Tex0();
    }
}

static inline void GXSetTexCoordGen(u8 dst_coord, u8 func, u8 src_param, u32 mtx) {
    GXSetTexCoordGen2(dst_coord, func, src_param, mtx, 0, GX_PTIDENTITY);
}

int main(void) {
    gc_safepoint();

    GXSetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);

    *(volatile u32 *)0x80300000 = 0xDEADBEEFu;
    *(volatile u32 *)0x80300004 = s_xf40;
    *(volatile u32 *)0x80300008 = s_xf50;
    *(volatile u32 *)0x8030000C = s_mat_idx_a;
    *(volatile u32 *)0x80300010 = s_xf24;

    {
        u32 off;
        for (off = 0x14; off < 0x40; off += 4) {
        *(volatile u32 *)(0x80300000u + off) = 0;
        }
    }

    while (1) gc_safepoint();
}
