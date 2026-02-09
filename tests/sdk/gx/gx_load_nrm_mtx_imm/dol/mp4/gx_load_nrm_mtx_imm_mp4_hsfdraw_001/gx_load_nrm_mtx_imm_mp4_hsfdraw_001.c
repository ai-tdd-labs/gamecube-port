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

static void GXLoadNrmMtxImm(f32 mtx[3][4], u32 id) {
    // Mirror decomp_mario_party_4/src/dolphin/gx/GXTransform.c:GXLoadNrmMtxImm.
    const u32 addr = id * 3u + 0x400u;
    const u32 reg = addr | 0x80000u;

    s_fifo_u8 = 0x10;
    s_fifo_u32 = reg;

    // WriteMTXPS3x3from3x4 writes the top-left 3x3 (9 floats).
    {
        u32 i = 0;
        u32 r, c;
        for (r = 0; r < 3; r++) {
            for (c = 0; c < 3; c++) {
                union { f32 f; u32 u; } u = { mtx[r][c] };
                s_fifo_mtx_words[i++] = u.u;
            }
        }
        for (; i < 12; i++) s_fifo_mtx_words[i] = 0;
    }
}

int main(void) {
    gc_safepoint();

    // Deterministic, non-identity 3x4 matrix (only 3x3 is consumed).
    static f32 mtx[3][4];
    u32 r, c;
    for (r = 0; r < 3; r++) {
        for (c = 0; c < 4; c++) {
            mtx[r][c] = 0.0f;
        }
    }
    mtx[0][0] = 1.0f;
    mtx[0][1] = 2.0f;
    mtx[0][2] = 3.0f;
    mtx[1][0] = 4.0f;
    mtx[1][1] = 5.0f;
    mtx[1][2] = 6.0f;
    mtx[2][0] = 7.0f;
    mtx[2][1] = 8.0f;
    mtx[2][2] = 9.0f;
    mtx[0][3] = 123.0f; // ignored
    mtx[1][3] = 456.0f; // ignored
    mtx[2][3] = 789.0f; // ignored

    GXLoadNrmMtxImm(mtx, 0);

    *(volatile u32 *)0x80300000 = 0xDEADBEEFu;
    *(volatile u32 *)0x80300004 = (u32)s_fifo_u8;
    *(volatile u32 *)0x80300008 = s_fifo_u32;

    // First 9 words prove packing order.
    *(volatile u32 *)0x8030000C = s_fifo_mtx_words[0];
    *(volatile u32 *)0x80300010 = s_fifo_mtx_words[1];
    *(volatile u32 *)0x80300014 = s_fifo_mtx_words[2];
    *(volatile u32 *)0x80300018 = s_fifo_mtx_words[3];
    *(volatile u32 *)0x8030001C = s_fifo_mtx_words[4];
    *(volatile u32 *)0x80300020 = s_fifo_mtx_words[5];
    *(volatile u32 *)0x80300024 = s_fifo_mtx_words[6];
    *(volatile u32 *)0x80300028 = s_fifo_mtx_words[7];
    *(volatile u32 *)0x8030002C = s_fifo_mtx_words[8];

    // Pad to 0x40 bytes.
    {
        u32 off;
        for (off = 0x30; off < 0x40; off += 4) {
            *(volatile u32 *)(0x80300000u + off) = 0;
        }
    }

    while (1) gc_safepoint();
}
