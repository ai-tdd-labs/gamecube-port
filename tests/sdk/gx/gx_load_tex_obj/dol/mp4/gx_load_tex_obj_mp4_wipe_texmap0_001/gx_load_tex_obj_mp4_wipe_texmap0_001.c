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
    u32 userData;
    u32 fmt;
    u32 tlutName;
    u16 loadCnt;
    u8 loadFmt;
    u8 flags;
} GXTexObj;

typedef struct {
    u32 image1;
    u32 image2;
    u16 sizeEven;
    u16 sizeOdd;
    u8 is32bMipmap;
    u8 isCached;
    u8 _pad[2];
} GXTexRegion;

static u32 s_mode0, s_mode1, s_image0, s_image1, s_image2, s_image3;
static GXTexRegion s_tex_regions[8];
static u32 s_next_tex_rgn;

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
    return (reg & 0xF8FFFFFFu) | (val & 0xFFFFFFFFu);
}

enum {
    GX_TF_RGB565 = 0x4,
    GX_CLAMP = 0,
    GX_TEXMAP0 = 0,
    GX_TEXCACHE_32K = 0,
};

static void GXInitTexObj(GXTexObj *obj, void *image_ptr, u16 width, u16 height, u32 format, u32 wrap_s, u32 wrap_t, u8 mipmap) {
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
    // RGB565 low nibble => loadFmt=2 rowT=2 colT=2
    t->loadFmt = 2;
    {
        u32 rowC = ((u32)width + (1u << 2) - 1u) >> 2;
        u32 colC = ((u32)height + (1u << 2) - 1u) >> 2;
        t->loadCnt = (u16)((rowC * colC) & 0x7FFFu);
    }
    t->flags |= 2;
}

static void GXInitTexCacheRegion(GXTexRegion *region, u8 is_32b_mipmap, u32 tmem_even, u32 size_even, u32 tmem_odd, u32 size_odd) {
    (void)size_odd;
    u32 WidthExp2 = 3;
    if (size_even == GX_TEXCACHE_32K) WidthExp2 = 3;
    region->image1 = 0;
    region->image1 = set_field(region->image1, 15, 0, tmem_even >> 5);
    region->image1 = set_field(region->image1, 3, 15, WidthExp2);
    region->image1 = set_field(region->image1, 3, 18, WidthExp2);
    region->image1 = set_field(region->image1, 1, 21, 0);

    region->image2 = 0;
    region->image2 = set_field(region->image2, 15, 0, tmem_odd >> 5);
    region->image2 = set_field(region->image2, 3, 15, WidthExp2);
    region->image2 = set_field(region->image2, 3, 18, WidthExp2);
    region->is32bMipmap = is_32b_mipmap;
    region->isCached = 1;
}

static GXTexRegion *GXDefaultTexRegionCallback(GXTexObj *t_obj, u32 unused) {
    // Mirror GXInit.c __GXDefaultTexRegionCallback for the non-CI formats used by MP4 wipe.
    (void)unused;
    if (t_obj->fmt != 0x8u && t_obj->fmt != 0x9u && t_obj->fmt != 0xAu) {
        return &s_tex_regions[s_next_tex_rgn++ & 7u];
    }
    return &s_tex_regions[0];
}

static void GXLoadTexObjPreLoaded(GXTexObj *obj, GXTexRegion *r, u32 id) {
    // Mirror GXTexture.c:GXLoadTexObjPreLoaded for the observable BP regs.
    // IDs for TEXMAP0:
    const u8 GXTexMode0Id = 0x80;
    const u8 GXTexMode1Id = 0x84;
    const u8 GXTexImage0Id = 0x88;
    const u8 GXTexImage1Id = 0x8C;
    const u8 GXTexImage2Id = 0x90;
    const u8 GXTexImage3Id = 0x94;
    (void)id;

    obj->mode0 = set_field(obj->mode0, 8, 24, GXTexMode0Id);
    obj->mode1 = set_field(obj->mode1, 8, 24, GXTexMode1Id);
    obj->image0 = set_field(obj->image0, 8, 24, GXTexImage0Id);
    r->image1 = set_field(r->image1, 8, 24, GXTexImage1Id);
    r->image2 = set_field(r->image2, 8, 24, GXTexImage2Id);
    obj->image3 = set_field(obj->image3, 8, 24, GXTexImage3Id);

    s_mode0 = obj->mode0;
    s_mode1 = obj->mode1;
    s_image0 = obj->image0;
    s_image1 = r->image1;
    s_image2 = r->image2;
    s_image3 = obj->image3;
}

static void GXInit(void) {
    // Minimal init: seed the default tex cache regions and reset the round-robin counter.
    s_next_tex_rgn = 0;
    u32 i;
    for (i = 0; i < 8; i++) {
        GXInitTexCacheRegion(&s_tex_regions[i], 0, i * 0x8000u, GX_TEXCACHE_32K, 0x80000u + i * 0x8000u, GX_TEXCACHE_32K);
    }
}

static void GXLoadTexObj(GXTexObj *obj, u32 id) {
    GXTexRegion *r = GXDefaultTexRegionCallback(obj, id);
    GXLoadTexObjPreLoaded(obj, r, id);
}

int main(void) {
    gc_safepoint();

    GXTexObj tex;
    void *img = (void *)0x80010000u;

    GXInit();
    GXInitTexObj(&tex, img, 640, 480, GX_TF_RGB565, GX_CLAMP, GX_CLAMP, 0);
    GXLoadTexObj(&tex, GX_TEXMAP0);

    *(volatile u32 *)0x80300000 = 0xDEADBEEFu;
    *(volatile u32 *)0x80300004 = s_mode0;
    *(volatile u32 *)0x80300008 = s_mode1;
    *(volatile u32 *)0x8030000C = s_image0;
    *(volatile u32 *)0x80300010 = s_image1;
    *(volatile u32 *)0x80300014 = s_image2;
    *(volatile u32 *)0x80300018 = s_image3;
    {
        u32 off;
        for (off = 0x1C; off < 0x40; off += 4) {
            *(volatile u32 *)(0x80300000u + off) = 0;
        }
    }

    while (1) gc_safepoint();
}
