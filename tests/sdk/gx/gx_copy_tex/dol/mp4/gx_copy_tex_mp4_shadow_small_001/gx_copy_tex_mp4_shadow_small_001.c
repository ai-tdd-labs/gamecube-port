#include <stdint.h>

typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;

static u32 s_cp_tex_src;
static u32 s_cp_tex_size;
static u32 s_cp_tex_stride;
static u32 s_cp_tex;
static u32 s_cp_tex_addr_reg;
static u32 s_last_ras_reg;
static u32 s_bp_sent_not;
static u32 s_zmode_reg;
static u32 s_cmode_reg;

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

static void GX_WRITE_RAS_REG(u32 v) { s_last_ras_reg = v; }

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

static void GXSetTexCopySrc(u16 left, u16 top, u16 wd, u16 ht) {
    s_cp_tex_src = 0;
    s_cp_tex_src = set_field(s_cp_tex_src, 10, 0, (u32)left);
    s_cp_tex_src = set_field(s_cp_tex_src, 10, 10, (u32)top);
    s_cp_tex_src = set_field(s_cp_tex_src, 8, 24, 0x49u);

    s_cp_tex_size = 0;
    s_cp_tex_size = set_field(s_cp_tex_size, 10, 0, (u32)(wd - 1u));
    s_cp_tex_size = set_field(s_cp_tex_size, 10, 10, (u32)(ht - 1u));
    s_cp_tex_size = set_field(s_cp_tex_size, 8, 24, 0x4Au);
}

static void GXSetTexCopyDst(u16 wd, u16 ht, u32 fmt, u32 mipmap) {
    u32 rowTiles, colTiles, cmpTiles;
    u32 peTexFmt = fmt & 0xFu;
    u32 peTexFmtH;

    // field15 default (GX_CTF_R8) => 2
    s_cp_tex = set_field(s_cp_tex, 2, 15, 2u);
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

static void GXSetZMode(u8 enable, u32 func, u8 update_enable) {
    // Pack a zmode reg like the SDK does (reg id 0x40).
    u32 reg = 0;
    reg = set_field(reg, 1, 0, (u32)enable);
    reg = set_field(reg, 3, 1, func & 7u);
    reg = set_field(reg, 1, 4, (u32)update_enable);
    reg = set_field(reg, 8, 24, 0x40u);
    s_zmode_reg = reg;
    GX_WRITE_RAS_REG(reg);
}

static void GXSetAlphaUpdate(u8 enable) {
    // For GXCopyTex(clear=1) the SDK writes a modified cmode0. We only model bits 0/1 and reg id 0x42.
    u32 reg = 0;
    reg = set_field(reg, 1, 0, (u32)enable);
    reg = set_field(reg, 1, 1, (u32)enable);
    reg = set_field(reg, 8, 24, 0x42u);
    s_cmode_reg = reg;
    GX_WRITE_RAS_REG(reg);
}

static void GXCopyTex(void *dest, u32 clear) {
    u32 phyAddr = ((u32)(uintptr_t)dest) & 0x3FFFFFFFu;
    u32 reg = 0;
    reg = set_field(reg, 21, 0, (phyAddr >> 5));
    reg = set_field(reg, 8, 24, 0x4Bu);
    s_cp_tex_addr_reg = reg;

    s_cp_tex = set_field(s_cp_tex, 1, 11, (u32)(clear != 0));
    s_cp_tex = set_field(s_cp_tex, 1, 14, 0u);
    s_cp_tex = set_field(s_cp_tex, 8, 24, 0x52u);

    GX_WRITE_RAS_REG(s_cp_tex_src);
    GX_WRITE_RAS_REG(s_cp_tex_size);
    GX_WRITE_RAS_REG(s_cp_tex_stride);
    GX_WRITE_RAS_REG(s_cp_tex_addr_reg);
    GX_WRITE_RAS_REG(s_cp_tex);
    if (clear != 0) {
        // In the real SDK these come from gx->zmode and gx->cmode0.
        // For this deterministic testcase, call the setters to get canonical packed values.
        GXSetZMode(1, 7, 1);
        GXSetAlphaUpdate(0);
    }
    s_bp_sent_not = 0;
}

int main(void) {
    gc_safepoint();

    const u16 unk_02 = 0x00F0;
    s_cp_tex = 0;
    s_bp_sent_not = 1;
    s_last_ras_reg = 0;
    s_zmode_reg = 0;
    s_cmode_reg = 0;

    GXSetTexCopySrc(0, 0, (u16)(unk_02 * 2), (u16)(unk_02 * 2));
    GXSetTexCopyDst(unk_02, unk_02, GX_CTF_R8, 1);
    GXCopyTex((void *)0x81234560u, 1);

    *(volatile u32 *)0x80300000 = 0xDEADBEEFu;
    *(volatile u32 *)0x80300004 = s_cp_tex_src;
    *(volatile u32 *)0x80300008 = s_cp_tex_size;
    *(volatile u32 *)0x8030000C = s_cp_tex_stride;
    *(volatile u32 *)0x80300010 = s_cp_tex_addr_reg;
    *(volatile u32 *)0x80300014 = s_cp_tex;
    *(volatile u32 *)0x80300018 = s_zmode_reg;
    // Log the last written RAS register (includes reg-id in bits 24..31).
    // This matches the host-side oracle, which tracks only the write stream.
    *(volatile u32 *)0x8030001C = s_last_ras_reg;

    while (1) gc_safepoint();
}
