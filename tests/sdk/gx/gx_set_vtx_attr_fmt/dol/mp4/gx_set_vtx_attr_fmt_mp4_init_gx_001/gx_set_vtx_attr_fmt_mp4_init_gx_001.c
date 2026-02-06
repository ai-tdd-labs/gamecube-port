#include <stdint.h>

typedef uint32_t u32;
typedef uint8_t u8;

typedef struct { u8 _dummy; } GXFifoObj;
static GXFifoObj s_fifo_obj;

static u32 s_vat_a[8];
static u32 s_vat_b[8];
static u32 s_vat_c[8];
static u32 s_dirty_state;
static u32 s_dirty_vat;

static inline u32 set_field(u32 reg, u32 size, u32 shift, u32 v) {
    const u32 mask = ((1u << size) - 1u) << shift;
    return (reg & ~mask) | ((v << shift) & mask);
}

enum { GX_VA_POS = 9 };
enum { GX_POS_XYZ = 1 };
enum { GX_F32 = 4 };

typedef u32 GXVtxFmt;

static inline void set_vat(u32 *va, u32 *vb, u32 *vc, u32 attr, u32 cnt, u32 type, u8 shft) {
    (void)vb; (void)vc;
    switch (attr) {
    case GX_VA_POS:
        *va = set_field(*va, 1, 0, cnt);
        *va = set_field(*va, 3, 1, type);
        *va = set_field(*va, 5, 4, shft);
        break;
    default:
        break;
    }
}

static void GXSetVtxAttrFmt(GXVtxFmt vtxfmt, u32 attr, u32 cnt, u32 type, u8 frac) {
    u32 *va = &s_vat_a[vtxfmt];
    u32 *vb = &s_vat_b[vtxfmt];
    u32 *vc = &s_vat_c[vtxfmt];
    set_vat(va, vb, vc, attr, cnt, type, frac);
    s_dirty_state |= 0x10u;
    s_dirty_vat |= (u32)(1u << (u8)vtxfmt);
}

static GXFifoObj *GXInit(void *base, u32 size) {
    (void)base; (void)size;
    u32 i;
    for (i = 0; i < 8; i++) { s_vat_a[i] = 0; s_vat_b[i] = 0; s_vat_c[i] = 0; }
    s_dirty_state = 0;
    s_dirty_vat = 0;
    return &s_fifo_obj;
}

int main(void) {
    static u8 fifo[0x20000];
    GXInit(fifo, sizeof(fifo));

    GXSetVtxAttrFmt(0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);

    *(volatile u32 *)0x80300000 = 0xDEADBEEFu;
    *(volatile u32 *)0x80300004 = s_vat_a[0];
    *(volatile u32 *)0x80300008 = s_vat_b[0];
    *(volatile u32 *)0x8030000C = s_vat_c[0];
    *(volatile u32 *)0x80300010 = s_dirty_state;
    *(volatile u32 *)0x80300014 = s_dirty_vat;
    while (1) {}
}
