#include <stdint.h>

typedef uint32_t u32;
typedef uint8_t u8;

typedef struct { u8 _dummy; } GXFifoObj;
static GXFifoObj s_fifo_obj;

static u32 s_vcd_lo;
static u32 s_vcd_hi;
static u32 s_has_nrms;
static u32 s_has_binrms;
static u32 s_dirty_state;

static inline u32 set_field(u32 reg, u32 size, u32 shift, u32 v) {
    const u32 mask = ((1u << size) - 1u) << shift;
    return (reg & ~mask) | ((v << shift) & mask);
}

static GXFifoObj *GXInit(void *base, u32 size) {
    (void)base; (void)size;
    s_vcd_lo = 0xFFFFFFFFu;
    s_vcd_hi = 0xFFFFFFFFu;
    s_has_nrms = 1;
    s_has_binrms = 1;
    s_dirty_state = 0;
    return &s_fifo_obj;
}

static void GXClearVtxDesc(void) {
    s_vcd_lo = 0;
    s_vcd_lo = set_field(s_vcd_lo, 2, 9, 1u);
    s_vcd_hi = 0;
    s_has_nrms = 0;
    s_has_binrms = 0;
    s_dirty_state |= 8u;
}

int main(void) {
    static u8 fifo[0x20000];
    GXInit(fifo, sizeof(fifo));

    GXClearVtxDesc();

    *(volatile u32 *)0x80300000 = 0xDEADBEEFu;
    *(volatile u32 *)0x80300004 = s_vcd_lo;
    *(volatile u32 *)0x80300008 = s_vcd_hi;
    *(volatile u32 *)0x8030000C = s_has_nrms;
    *(volatile u32 *)0x80300010 = s_has_binrms;
    *(volatile u32 *)0x80300014 = s_dirty_state;
    while (1) {}
}
