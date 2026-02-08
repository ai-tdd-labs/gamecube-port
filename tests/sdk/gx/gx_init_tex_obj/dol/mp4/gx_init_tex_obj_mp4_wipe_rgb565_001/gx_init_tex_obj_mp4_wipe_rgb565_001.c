#include <stdint.h>
#include <string.h>

typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;

typedef struct {
    u32 mode0;
    u32 mode1;
    u32 image0;
    u32 image3;
    u32 userData;   // 32-bit pointer slot (keep 0x20-byte layout on host and PPC)
    u32 fmt;
    u32 tlutName;
    u16 loadCnt;
    u8 loadFmt;
    u8 flags;
} GXTexObj;

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

static inline u32 some_set_reg_macro(u32 reg, u32 val) {
    // Mirror GXTexture.c SOME_SET_REG_MACRO (rlwinm(...,0,27,23) clears bits 24..26).
    return (reg & 0xF8FFFFFFu) | (val & 0xFFFFFFFFu);
}

enum {
    GX_TF_RGB565 = 0x4,
    GX_CLAMP = 0,
};

static void GXInitTexObj(GXTexObj *obj, void *image_ptr, u16 width, u16 height, u32 format, u32 wrap_s, u32 wrap_t, u8 mipmap) {
    // Mirror decomp_mario_party_4/src/dolphin/gx/GXTexture.c:GXInitTexObj (observable packing).
    (void)mipmap;
    GXTexObj *t = obj;
    memset(t, 0, 0x20);

    t->mode0 = set_field(t->mode0, 2, 0, wrap_s);
    t->mode0 = set_field(t->mode0, 2, 2, wrap_t);
    t->mode0 = set_field(t->mode0, 1, 4, 1);
    t->mode0 = some_set_reg_macro(t->mode0, 0x80);

    t->fmt = format;
    t->image0 = set_field(t->image0, 10, 0, (u32)(width - 1u));
    t->image0 = set_field(t->image0, 10, 10, (u32)(height - 1u));
    t->image0 = set_field(t->image0, 4, 20, (u32)(format & 0xFu));

    {
        u32 imageBase = ((u32)(uintptr_t)image_ptr >> 5) & 0x01FFFFFFu;
        t->image3 = set_field(t->image3, 21, 0, imageBase);
    }

    // format&0xF == 4 (RGB565) => loadFmt=2, rowT=2, colT=2
    t->loadFmt = 2;
    {
        u32 rowC = ((u32)width + (1u << 2) - 1u) >> 2;
        u32 colC = ((u32)height + (1u << 2) - 1u) >> 2;
        t->loadCnt = (u16)((rowC * colC) & 0x7FFFu);
    }
    t->flags |= 2;
}

int main(void) {
    gc_safepoint();

    GXTexObj tex;
    void *img = (void *)0x80010000u; // 32B aligned
    GXInitTexObj(&tex, img, 640, 480, GX_TF_RGB565, GX_CLAMP, GX_CLAMP, 0);

    *(volatile u32 *)0x80300000 = 0xDEADBEEFu;
    *(volatile u32 *)0x80300004 = tex.mode0;
    *(volatile u32 *)0x80300008 = tex.image0;
    *(volatile u32 *)0x8030000C = tex.image3;
    *(volatile u32 *)0x80300010 = tex.fmt;
    *(volatile u32 *)0x80300014 = (u32)tex.loadCnt;
    *(volatile u32 *)0x80300018 = ((u32)tex.loadFmt << 8) | (u32)tex.flags;
    {
        u32 off;
        for (off = 0x1C; off < 0x40; off += 4) {
            *(volatile u32 *)(0x80300000u + off) = 0;
        }
    }

    while (1) gc_safepoint();
}

