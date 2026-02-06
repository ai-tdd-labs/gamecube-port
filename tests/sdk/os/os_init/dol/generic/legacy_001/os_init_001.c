#include <stdint.h>
#ifdef GC_HOST_TEST
#include "runtime/os.h"
extern uint8_t *gc_test_ram;
#define RESULT_PTR() ((volatile uint8_t *)(gc_test_ram + 0x00300000))
typedef uint32_t u32;
static inline void store_be32(volatile uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)((v >> 24) & 0xFF);
    p[1] = (uint8_t)((v >> 16) & 0xFF);
    p[2] = (uint8_t)((v >> 8) & 0xFF);
    p[3] = (uint8_t)(v & 0xFF);
}
#else
#include <dolphin/os.h>
#include <dolphin/types.h>
#define RESULT_PTR() ((volatile u32 *)0x80300000)
#endif

int main(void) {
    void *arena_lo;
    void *arena_hi;

    OSInit();
    arena_lo = OSGetArenaLo();
    arena_hi = OSGetArenaHi();

#ifdef GC_HOST_TEST
    volatile uint8_t *out = RESULT_PTR();
    store_be32(out + 0, 0xDEADBEEF);
    store_be32(out + 4, (u32)(uintptr_t)arena_lo);
    store_be32(out + 8, (u32)(uintptr_t)arena_hi);
#else
    volatile u32 *out = RESULT_PTR();
    out[0] = 0xDEADBEEF;
    out[1] = (u32)(uintptr_t)arena_lo;
    out[2] = (u32)(uintptr_t)arena_hi;
#endif

#ifndef GC_HOST_TEST
    while (1) {}
#endif
    return 0;
}
