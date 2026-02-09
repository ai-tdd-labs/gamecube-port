#include <stdint.h>

typedef uint32_t u32;
typedef float f32;
typedef uint8_t u8;

enum {
    GX_MTX3x4 = 0,
    GX_MTX2x4 = 1,
    GX_TEXMTX7 = 51,
};

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

static void GXLoadTexMtxImm(f32 mtx[][4], u32 id, u32 type) {
    // Mirror decomp_mario_party_4/src/dolphin/gx/GXTransform.c:GXLoadTexMtxImm.
    u32 addr;
    u32 count;

    if (id >= 64u) {
        addr = (id - 64u) * 4u + 0x500u;
        // SDK asserts type == GX_MTX3x4 here.
    } else {
        addr = id * 4u;
    }

    count = (type == GX_MTX2x4) ? 8u : 12u;
    const u32 reg = addr | ((count - 1u) << 16);

    s_fifo_u8 = 0x10;
    s_fifo_u32 = reg;

    u32 i = 0;
    u32 r, c;
    if (type == GX_MTX3x4) {
        for (r = 0; r < 3; r++) {
            for (c = 0; c < 4; c++) {
                union { f32 f; u32 u; } u = { mtx[r][c] };
                s_fifo_mtx_words[i++] = u.u;
            }
        }
    } else {
        for (r = 0; r < 2; r++) {
            for (c = 0; c < 4; c++) {
                union { f32 f; u32 u; } u = { mtx[r][c] };
                s_fifo_mtx_words[i++] = u.u;
            }
        }
    }
    for (; i < 12; i++) s_fifo_mtx_words[i] = 0;
}

int main(void) {
    gc_safepoint();

    // Deterministic 2x4 matrix.
    static f32 mtx[2][4];
    u32 r, c;
    for (r = 0; r < 2; r++) {
        for (c = 0; c < 4; c++) {
            mtx[r][c] = (f32)(r * 10 + c + 1);
        }
    }

    GXLoadTexMtxImm((f32 (*)[4])mtx, GX_TEXMTX7, GX_MTX2x4);

    *(volatile u32 *)0x80300000 = 0xDEADBEEFu;
    *(volatile u32 *)0x80300004 = (u32)s_fifo_u8;
    *(volatile u32 *)0x80300008 = s_fifo_u32;

    // First 8 words prove 2x4 packing.
    u32 i;
    for (i = 0; i < 8; i++) {
        *(volatile u32 *)(0x8030000Cu + i * 4u) = s_fifo_mtx_words[i];
    }
    // Pad to 0x40.
    u32 off;
    for (off = 0x2C; off < 0x40; off += 4) {
        *(volatile u32 *)(0x80300000u + off) = 0;
    }

    while (1) gc_safepoint();
}
