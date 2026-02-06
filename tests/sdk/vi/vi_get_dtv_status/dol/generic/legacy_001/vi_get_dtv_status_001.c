#ifdef GC_HOST_TEST
#include <stdint.h>
#include "runtime/vi.h"
extern uint8_t *gc_test_ram;
extern uint32_t gc_vi_disable_calls;
extern uint32_t gc_vi_restore_calls;
extern uint16_t gc_vi_regs[64];
#define VI_DTV_STAT 55
#define RESULT_PTR() ((volatile uint8_t *)(gc_test_ram + 0x00300000))
typedef uint32_t u32;
static inline void store_be32(volatile uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)((v >> 24) & 0xFF);
    p[1] = (uint8_t)((v >> 16) & 0xFF);
    p[2] = (uint8_t)((v >> 8) & 0xFF);
    p[3] = (uint8_t)(v & 0xFF);
}
#else
#include <stdint.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif
#define RESULT_PTR() ((volatile u32 *)0x80300000)

#define VI_DTV_STAT 55
static u16 __VIRegs[64];
static u32 disable_calls = 0;
static u32 restore_calls = 0;

void OSInit(void) {}

int OSDisableInterrupts(void) {
    disable_calls++;
    return 1;
}

void OSRestoreInterrupts(int enabled) {
    (void)enabled;
    restore_calls++;
}

u32 VIGetDTVStatus(void) {
    u32 stat;
    int interrupt;

    interrupt = OSDisableInterrupts();
    stat = (__VIRegs[VI_DTV_STAT] & 3);
    OSRestoreInterrupts(interrupt);
    return (stat & 1);
}
#endif

int main(void) {
#ifndef GC_HOST_TEST
    OSInit();
    __VIRegs[VI_DTV_STAT] = 3;
    disable_calls = 0;
    restore_calls = 0;
#endif

#ifdef GC_HOST_TEST
    gc_vi_disable_calls = 0;
    gc_vi_restore_calls = 0;
    gc_vi_regs[VI_DTV_STAT] = 3;
#endif
    u32 stat = VIGetDTVStatus();

#ifdef GC_HOST_TEST
    volatile uint8_t *result = RESULT_PTR();
    store_be32(result + 0x00, 0xDEADBEEF);
    store_be32(result + 0x04, stat);
    store_be32(result + 0x08, gc_vi_disable_calls);
    store_be32(result + 0x0C, gc_vi_restore_calls);
#else
    volatile u32 *result = RESULT_PTR();
    result[0] = 0xDEADBEEF;
    result[1] = stat;
    result[2] = disable_calls;
    result[3] = restore_calls;
    while (1) {}
#endif
    return 0;
}
