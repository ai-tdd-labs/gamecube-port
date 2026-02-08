#include <stdint.h>

typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;

static u32 s_cp_tex;
static u32 s_cp_tex_stride;
static u32 s_cp_tex_z;

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

enum { GX_CTF_R8 = 0x4 };

static void get_image_tile_count(u32 fmt, u16 wd, u16 ht, u32 *rowTiles, u32 *colTiles, u32 *cmpTiles) {
    u16 tileW = 8;
    u16 tileH = 4;
    u32 cmp = 1;
    if (fmt == GX_CTF_R8) { tileW = 8; tileH = 4; cmp = 1; }
    *rowTiles = (wd + tileW - 1u) / tileW;
    *colTiles = (ht + tileH - 1u) / tileH;
    *cmpTiles = cmp;
}

static void GXSetTexCopyDst(u16 wd, u16 ht, u32 fmt, u32 mipmap) {
    u32 rowTiles, colTiles, cmpTiles;
    u32 peTexFmt = fmt & 0xFu;
    u32 peTexFmtH;

    s_cp_tex_z = 0;

    // GX_CTF_R8 falls into default => field15 = 2.
    s_cp_tex = set_field(s_cp_tex, 2, 15, 2u);

    s_cp_tex_z = (fmt & 0x10u) == 0x10u;
    peTexFmtH = (peTexFmt >> 3) & 1u;
    s_cp_tex = set_field(s_cp_tex, 1, 3, peTexFmtH);
    peTexFmt = peTexFmt & 7u;

    get_image_tile_count(fmt, wd, ht, &rowTiles, &colTiles, &cmpTiles);

    s_cp_tex_stride = 0;
    s_cp_tex_stride = set_field(s_cp_tex_stride, 10, 0, rowTiles * cmpTiles);
    s_cp_tex_stride = set_field(s_cp_tex_stride, 8, 24, 0x4Du);

    s_cp_tex = set_field(s_cp_tex, 1, 9, (u32)(mipmap != 0));
    s_cp_tex = set_field(s_cp_tex, 3, 4, peTexFmt);
}

int main(void) {
    gc_safepoint();

    const u16 unk_02 = 0x00F0;
    s_cp_tex = 0;
    s_cp_tex_stride = 0;
    s_cp_tex_z = 0;
    GXSetTexCopyDst(unk_02, unk_02, GX_CTF_R8, 1);

    *(volatile u32 *)0x80300000 = 0xDEADBEEFu;
    *(volatile u32 *)0x80300004 = s_cp_tex;
    *(volatile u32 *)0x80300008 = s_cp_tex_stride;
    *(volatile u32 *)0x8030000C = s_cp_tex_z;
    *(volatile u32 *)0x80300010 = 0;

    while (1) gc_safepoint();
}

