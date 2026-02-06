#include <stdint.h>

typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;

typedef struct { u8 _dummy; } GXFifoObj;
static GXFifoObj s_fifo_obj;

static u32 s_vcd_lo;
static u32 s_vcd_hi;
static u32 s_has_nrms;
static u32 s_has_binrms;
static u32 s_nrm_type;
static u32 s_dirty_state;

static inline u32 set_field(u32 reg, u32 size, u32 shift, u32 v) {
    const u32 mask = ((1u << size) - 1u) << shift;
    return (reg & ~mask) | ((v << shift) & mask);
}

enum {
    GX_VA_PNMTXIDX = 0,
    GX_VA_TEX0MTXIDX = 1,
    GX_VA_TEX1MTXIDX = 2,
    GX_VA_TEX2MTXIDX = 3,
    GX_VA_TEX3MTXIDX = 4,
    GX_VA_TEX4MTXIDX = 5,
    GX_VA_TEX5MTXIDX = 6,
    GX_VA_TEX6MTXIDX = 7,
    GX_VA_TEX7MTXIDX = 8,
    GX_VA_POS = 9,
    GX_VA_NRM = 10,
    GX_VA_CLR0 = 11,
    GX_VA_CLR1 = 12,
    GX_VA_TEX0 = 13,
    GX_VA_TEX1 = 14,
    GX_VA_TEX2 = 15,
    GX_VA_TEX3 = 16,
    GX_VA_TEX4 = 17,
    GX_VA_TEX5 = 18,
    GX_VA_TEX6 = 19,
    GX_VA_TEX7 = 20,
    GX_VA_NBT = 25,
};

enum {
    GX_NONE = 0,
    GX_DIRECT = 1,
    GX_INDEX8 = 2,
    GX_INDEX16 = 3,
};

static void set_vcd_attr(u32 attr, u32 type) {
    switch (attr) {
    case GX_VA_PNMTXIDX:   s_vcd_lo = set_field(s_vcd_lo, 1, 0, type); break;
    case GX_VA_TEX0MTXIDX: s_vcd_lo = set_field(s_vcd_lo, 1, 1, type); break;
    case GX_VA_TEX1MTXIDX: s_vcd_lo = set_field(s_vcd_lo, 1, 2, type); break;
    case GX_VA_TEX2MTXIDX: s_vcd_lo = set_field(s_vcd_lo, 1, 3, type); break;
    case GX_VA_TEX3MTXIDX: s_vcd_lo = set_field(s_vcd_lo, 1, 4, type); break;
    case GX_VA_TEX4MTXIDX: s_vcd_lo = set_field(s_vcd_lo, 1, 5, type); break;
    case GX_VA_TEX5MTXIDX: s_vcd_lo = set_field(s_vcd_lo, 1, 6, type); break;
    case GX_VA_TEX6MTXIDX: s_vcd_lo = set_field(s_vcd_lo, 1, 7, type); break;
    case GX_VA_TEX7MTXIDX: s_vcd_lo = set_field(s_vcd_lo, 1, 8, type); break;
    case GX_VA_POS:        s_vcd_lo = set_field(s_vcd_lo, 2, 9, type); break;
    case GX_VA_NRM:
        if (type != GX_NONE) { s_has_nrms = 1; s_has_binrms = 0; s_nrm_type = type; }
        else { s_has_nrms = 0; }
        break;
    case GX_VA_NBT:
        if (type != GX_NONE) { s_has_binrms = 1; s_has_nrms = 0; s_nrm_type = type; }
        else { s_has_binrms = 0; }
        break;
    case GX_VA_CLR0: s_vcd_lo = set_field(s_vcd_lo, 2, 13, type); break;
    case GX_VA_CLR1: s_vcd_lo = set_field(s_vcd_lo, 2, 15, type); break;
    case GX_VA_TEX0: s_vcd_hi = set_field(s_vcd_hi, 2, 0, type); break;
    case GX_VA_TEX1: s_vcd_hi = set_field(s_vcd_hi, 2, 2, type); break;
    case GX_VA_TEX2: s_vcd_hi = set_field(s_vcd_hi, 2, 4, type); break;
    case GX_VA_TEX3: s_vcd_hi = set_field(s_vcd_hi, 2, 6, type); break;
    case GX_VA_TEX4: s_vcd_hi = set_field(s_vcd_hi, 2, 8, type); break;
    case GX_VA_TEX5: s_vcd_hi = set_field(s_vcd_hi, 2, 10, type); break;
    case GX_VA_TEX6: s_vcd_hi = set_field(s_vcd_hi, 2, 12, type); break;
    case GX_VA_TEX7: s_vcd_hi = set_field(s_vcd_hi, 2, 14, type); break;
    default: break;
    }
}

static void GXSetVtxDesc(u32 attr, u32 type) {
    set_vcd_attr(attr, type);
    if (s_has_nrms || s_has_binrms) {
        s_vcd_lo = set_field(s_vcd_lo, 2, 11, s_nrm_type);
    } else {
        s_vcd_lo = set_field(s_vcd_lo, 2, 11, 0);
    }
    s_dirty_state |= 8u;
}

static GXFifoObj *GXInit(void *base, u32 size) {
    (void)base; (void)size;
    s_vcd_lo = 0;
    s_vcd_hi = 0;
    s_has_nrms = 0;
    s_has_binrms = 0;
    s_nrm_type = 0;
    s_dirty_state = 0;
    return &s_fifo_obj;
}

int main(void) {
    static u8 fifo[0x20000];
    GXInit(fifo, sizeof(fifo));

    // Representative MP4 usage: position is direct.
    GXSetVtxDesc(GX_VA_POS, GX_DIRECT);

    *(volatile u32 *)0x80300000 = 0xDEADBEEFu;
    *(volatile u32 *)0x80300004 = s_vcd_lo;
    *(volatile u32 *)0x80300008 = s_vcd_hi;
    *(volatile u32 *)0x8030000C = s_has_nrms;
    *(volatile u32 *)0x80300010 = s_has_binrms;
    *(volatile u32 *)0x80300014 = s_nrm_type;
    *(volatile u32 *)0x80300018 = s_dirty_state;
    while (1) {}
}
