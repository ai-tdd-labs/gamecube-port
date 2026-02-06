#ifdef GC_HOST_TEST
#include <stdint.h>
#include "runtime/runtime.h"
#include "runtime/vi.h"
extern uint8_t *gc_test_ram;
typedef uint32_t u32;
#define RESULT_PTR() ((volatile uint8_t *)(gc_test_ram + 0x00300000))
#define TEST_OS_INIT() gc_runtime_init()
static inline void store_be32(volatile uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)((v >> 24) & 0xFF);
    p[1] = (uint8_t)((v >> 16) & 0xFF);
    p[2] = (uint8_t)((v >> 8) & 0xFF);
    p[3] = (uint8_t)(v & 0xFF);
}
#else
#include <dolphin/types.h>
#include <dolphin/vi.h>
void OSInit(void);
#define RESULT_PTR() ((volatile u32 *)0x80300000)
#define TEST_OS_INIT() OSInit()
#endif

#ifndef GC_HOST_TEST
static u32 s_tv_format;

void VIInit(void) {
    s_tv_format = VI_NTSC;
}

u32 VIGetTvFormat(void) {
    return s_tv_format;
}
#endif

int main(void) {
    TEST_OS_INIT();
    VIInit();
    u32 fmt = VIGetTvFormat();

#ifdef GC_HOST_TEST
    volatile uint8_t *result = RESULT_PTR();
    store_be32(result + 0, 0xDEADBEEF);
    store_be32(result + 4, fmt);
    return 0;
#else
    volatile u32 *result = RESULT_PTR();
    result[0] = 0xDEADBEEF;
    result[1] = fmt;
    while (1) {
    }
#endif
}
