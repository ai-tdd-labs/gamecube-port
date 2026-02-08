#include <stdint.h>

typedef uint32_t u32;

static u32 s_last_ras_reg;
static u32 s_draw_done_flag;
static u32 s_calls;

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

static void GX_WRITE_RAS_REG(u32 v) { s_last_ras_reg = v; }

static void GXSetDrawDone(void) {
    // Mirror the observable BP write from decomp_mario_party_4/src/dolphin/gx/GXMisc.c:GXSetDrawDone.
    // We intentionally do not model interrupts/threads in this deterministic test.
    s_calls++;
    GX_WRITE_RAS_REG(0x45000002u);
    s_draw_done_flag = 0;
}

int main(void) {
    gc_safepoint();
    s_last_ras_reg = 0;
    s_draw_done_flag = 0xFFFFFFFFu;
    s_calls = 0;

    GXSetDrawDone();

    *(volatile u32*)0x80300000 = 0xDEADBEEFu;
    *(volatile u32*)0x80300004 = s_calls;
    *(volatile u32*)0x80300008 = s_last_ras_reg;
    *(volatile u32*)0x8030000C = s_draw_done_flag;
    while (1) { gc_safepoint(); }
}
