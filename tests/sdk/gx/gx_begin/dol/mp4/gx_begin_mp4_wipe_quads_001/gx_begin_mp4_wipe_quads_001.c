#include <stdint.h>

typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;

static u8 s_fifo_u8;
static u16 s_fifo_u16;

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

// Minimal subset: MP4 uses GXBegin(GX_QUADS, GX_VTXFMT0, 4).
enum {
    GX_QUADS = 0x80,
    GX_VTXFMT0 = 0,
};

static void GXBegin(u8 type, u8 vtxfmt, u16 nverts) {
    // Mirror observable write in GXGeometry.c: GX_WRITE_U8(vtxfmt | type); GX_WRITE_U16(nverts);
    s_fifo_u8 = (u8)(vtxfmt | type);
    s_fifo_u16 = nverts;
}

int main(void) {
    gc_safepoint();

    GXBegin(GX_QUADS, GX_VTXFMT0, 4);

    *(volatile u32 *)0x80300000 = 0xDEADBEEFu;
    *(volatile u32 *)0x80300004 = (u32)s_fifo_u8;
    *(volatile u32 *)0x80300008 = (u32)s_fifo_u16;
    // Pad to 0x40 bytes.
    {
        u32 off;
        for (off = 0x0C; off < 0x40; off += 4) {
            *(volatile u32 *)(0x80300000u + off) = 0;
        }
    }

    while (1) gc_safepoint();
}

