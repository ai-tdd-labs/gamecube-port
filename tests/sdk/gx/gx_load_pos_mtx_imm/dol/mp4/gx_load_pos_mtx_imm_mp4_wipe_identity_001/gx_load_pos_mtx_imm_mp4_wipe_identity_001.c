#include <stdint.h>

typedef uint32_t u32;
typedef float f32;
typedef uint8_t u8;

static u8 s_fifo_u8;
static u32 s_fifo_u32;
static u32 s_fifo_mtx_words[12];

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

static void GXLoadPosMtxImm(f32 mtx[3][4], u32 id) {
    // Mirror decomp_mario_party_4/src/dolphin/gx/GXTransform.c:GXLoadPosMtxImm.
    const u32 addr = id * 4u;
    const u32 reg = addr | 0xB0000u;

    s_fifo_u8 = 0x10;
    s_fifo_u32 = reg;

    // WriteMTXPS4x3 writes 12 floats row-major.
    {
        u32 i = 0;
        u32 r, c;
        for (r = 0; r < 3; r++) {
            for (c = 0; c < 4; c++) {
                union { f32 f; u32 u; } u = { mtx[r][c] };
                s_fifo_mtx_words[i++] = u.u;
            }
        }
    }
}

int main(void) {
    gc_safepoint();

    // Identity modelview as used in wipe.c.
    static f32 mtx[3][4];
    u32 r, c;
    for (r = 0; r < 3; r++) {
        for (c = 0; c < 4; c++) {
            mtx[r][c] = 0.0f;
        }
    }
    mtx[0][0] = 1.0f;
    mtx[1][1] = 1.0f;
    mtx[2][2] = 1.0f;

    GXLoadPosMtxImm(mtx, 0); // GX_PNMTX0

    *(volatile u32 *)0x80300000 = 0xDEADBEEFu;
    *(volatile u32 *)0x80300004 = (u32)s_fifo_u8;
    *(volatile u32 *)0x80300008 = s_fifo_u32;

    // First 5 words is enough to prove the row-major packing (1,0,0,0, 0...).
    *(volatile u32 *)0x8030000C = s_fifo_mtx_words[0];
    *(volatile u32 *)0x80300010 = s_fifo_mtx_words[1];
    *(volatile u32 *)0x80300014 = s_fifo_mtx_words[2];
    *(volatile u32 *)0x80300018 = s_fifo_mtx_words[3];
    *(volatile u32 *)0x8030001C = s_fifo_mtx_words[4];

    // Pad to 0x40 bytes.
    {
        u32 off;
        for (off = 0x20; off < 0x40; off += 4) {
            *(volatile u32 *)(0x80300000u + off) = 0;
        }
    }

    while (1) gc_safepoint();
}
