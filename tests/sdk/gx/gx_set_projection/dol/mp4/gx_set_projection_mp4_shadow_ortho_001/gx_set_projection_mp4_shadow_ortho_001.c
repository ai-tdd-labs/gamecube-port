#include <stdint.h>
#include <string.h>

typedef uint32_t u32;
typedef float f32;

typedef enum {
    GX_PERSPECTIVE = 0,
    GX_ORTHOGRAPHIC = 1,
} GXProjectionType;

static u32 s_proj_type;
static u32 s_proj_mtx_bits[6];
static u32 s_xf_regs[64]; // we only use indices 32..38
static u32 s_bp_sent_not;

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

static inline void write_xf(u32 idx, u32 bits) {
    if (idx < 64) s_xf_regs[idx] = bits;
}

static void GXSetProjection(f32 mtx[4][4], GXProjectionType type) {
    // Mirror decomp_mario_party_4/src/dolphin/gx/GXTransform.c:GXSetProjection observable behavior.
    f32 p0 = mtx[0][0];
    f32 p2 = mtx[1][1];
    f32 p4 = mtx[2][2];
    f32 p5 = mtx[2][3];
    f32 p1;
    f32 p3;

    s_proj_type = (u32)type;
    if (type == GX_ORTHOGRAPHIC) {
        p1 = mtx[0][3];
        p3 = mtx[1][3];
    } else {
        p1 = mtx[0][2];
        p3 = mtx[1][2];
    }

    __builtin_memcpy(&s_proj_mtx_bits[0], &p0, sizeof(u32));
    __builtin_memcpy(&s_proj_mtx_bits[1], &p1, sizeof(u32));
    __builtin_memcpy(&s_proj_mtx_bits[2], &p2, sizeof(u32));
    __builtin_memcpy(&s_proj_mtx_bits[3], &p3, sizeof(u32));
    __builtin_memcpy(&s_proj_mtx_bits[4], &p4, sizeof(u32));
    __builtin_memcpy(&s_proj_mtx_bits[5], &p5, sizeof(u32));

    // __GXSetProjection observable writes (XF regs 32..38).
    write_xf(32, s_proj_mtx_bits[0]);
    write_xf(33, s_proj_mtx_bits[1]);
    write_xf(34, s_proj_mtx_bits[2]);
    write_xf(35, s_proj_mtx_bits[3]);
    write_xf(36, s_proj_mtx_bits[4]);
    write_xf(37, s_proj_mtx_bits[5]);
    write_xf(38, s_proj_type);

    s_bp_sent_not = 1;
}

static void fill_ortho_0_1(f32 mtx[4][4]) {
    memset(mtx, 0, sizeof(f32) * 16);
    mtx[0][0] = 2.0f;
    mtx[0][3] = -1.0f;
    mtx[1][1] = 2.0f;
    mtx[1][3] = -1.0f;
    mtx[2][2] = -2.0f;
    mtx[2][3] = -1.0f;
}

int main(void) {
    gc_safepoint();

    f32 mtx[4][4];
    fill_ortho_0_1(mtx);

    s_proj_type = 0;
    {
        u32 i;
        for (i = 0; i < 6; i++) s_proj_mtx_bits[i] = 0;
        for (i = 0; i < 64; i++) s_xf_regs[i] = 0;
    }
    s_bp_sent_not = 0;

    GXSetProjection(mtx, GX_ORTHOGRAPHIC);

    *(volatile u32 *)0x80300000 = 0xDEADBEEFu;
    *(volatile u32 *)0x80300004 = s_proj_type;
    *(volatile u32 *)0x80300008 = s_proj_mtx_bits[0];
    *(volatile u32 *)0x8030000C = s_proj_mtx_bits[1];
    *(volatile u32 *)0x80300010 = s_proj_mtx_bits[2];
    *(volatile u32 *)0x80300014 = s_proj_mtx_bits[3];
    *(volatile u32 *)0x80300018 = s_proj_mtx_bits[4];
    *(volatile u32 *)0x8030001C = s_proj_mtx_bits[5];
    *(volatile u32 *)0x80300020 = s_xf_regs[32];
    *(volatile u32 *)0x80300024 = s_xf_regs[33];
    *(volatile u32 *)0x80300028 = s_xf_regs[34];
    *(volatile u32 *)0x8030002C = s_xf_regs[35];
    *(volatile u32 *)0x80300030 = s_xf_regs[36];
    *(volatile u32 *)0x80300034 = s_xf_regs[37];
    *(volatile u32 *)0x80300038 = s_xf_regs[38];
    *(volatile u32 *)0x8030003C = s_bp_sent_not;

    while (1) gc_safepoint();
}
