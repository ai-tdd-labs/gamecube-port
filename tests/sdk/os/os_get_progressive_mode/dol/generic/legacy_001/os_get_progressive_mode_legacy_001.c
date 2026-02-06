#ifdef GC_HOST_TEST
#include <stdint.h>
#include "runtime/os.h"
extern uint8_t *gc_test_ram;
extern uint32_t gc_sram_unlock_calls;
extern uint8_t gc_sram_flags;
#define RESULT_PTR() ((volatile uint8_t *)(gc_test_ram + 0x00300000))
typedef uint32_t u32;
static inline void store_be32(volatile uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)((v >> 24) & 0xFF);
    p[1] = (uint8_t)((v >> 16) & 0xFF);
    p[2] = (uint8_t)((v >> 8) & 0xFF);
    p[3] = (uint8_t)(v & 0xFF);
}
#else
#include <dolphin/types.h>
#define RESULT_PTR() ((volatile u32 *)0x80300000)

void OSInit(void) {}

typedef struct OSSram {
    u8 flags;
} OSSram;

static OSSram sram;
static u32 unlock_calls = 0;

OSSram *__OSLockSramHACK(void) {
    return &sram;
}

void __OSUnlockSram(BOOL commit) {
    (void)commit;
    unlock_calls++;
}

u32 OSGetProgressiveMode(void) {
    OSSram *s = __OSLockSramHACK();
    u32 mode = (s->flags & 0x80) >> 7;
    __OSUnlockSram(FALSE);
    return mode;
}
#endif

int main(void) {
    u32 mode;

#ifndef GC_HOST_TEST
    OSInit();
    sram.flags = 0x80;
    unlock_calls = 0;
#else
    gc_sram_flags = 0x80;
    gc_sram_unlock_calls = 0;
#endif

    mode = OSGetProgressiveMode();

#ifdef GC_HOST_TEST
    volatile uint8_t *result = RESULT_PTR();
    store_be32(result + 0x00, 0xDEADBEEF);
    store_be32(result + 0x04, mode);
    store_be32(result + 0x08, gc_sram_unlock_calls);
#else
    volatile u32 *result = RESULT_PTR();
    result[0] = 0xDEADBEEF;
    result[1] = mode;
    result[2] = unlock_calls;
    while (1) {}
#endif
    return 0;
}
