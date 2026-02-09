#include <stdint.h>

typedef uint32_t u32;
typedef uint8_t u8;
typedef uint16_t u16;

typedef struct { u8 _dummy; } GXFifoObj;
static GXFifoObj s_fifo_obj;

static u32 s_x;
static u32 s_y;

static inline void gc_safepoint(void) {
    u32 msr;
    __asm__ volatile("mfmsr %0" : "=r"(msr));
    msr &= ~0x8000u; // MSR[EE]
    __asm__ volatile("mtmsr %0" :: "r"(msr));
    __asm__ volatile("lis 3, 0x7fff; ori 3, 3, 0xffff; mtdec 3");
}

static void GXPosition2u16(u16 x, u16 y) {
    s_x = (u32)x;
    s_y = (u32)y;
}

static GXFifoObj *GXInit(void *base, u32 size) {
    (void)base;
    (void)size;
    s_x = s_y = 0;
    return &s_fifo_obj;
}

int main(void) {
    static u8 fifo[0x20000];
    gc_safepoint();
    GXInit(fifo, sizeof(fifo));

    // Wipe code uses u16 coords (no sign extension).
    GXPosition2u16((u16)0x1234u, (u16)0xFFFFu);

    // Unique marker for this test to avoid confusing stale values across runs.
    *(volatile u32 *)0x80300000 = 0xDEADBE02u;
    *(volatile u32 *)0x80300004 = s_x;
    *(volatile u32 *)0x80300008 = s_y;
    while (1) gc_safepoint();
}
