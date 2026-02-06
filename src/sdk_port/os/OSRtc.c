#include <stdint.h>

// Minimal SRAM + OSGetProgressiveMode port for host scenarios.

typedef uint8_t u8;
typedef uint32_t u32;
typedef int BOOL;
#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

u8 gc_sram_flags;
u32 gc_sram_unlock_calls;

typedef struct OSSram {
    u8 flags;
} OSSram;

static OSSram s_sram;

OSSram *__OSLockSramHACK(void) {
    s_sram.flags = gc_sram_flags;
    return &s_sram;
}

void __OSUnlockSram(BOOL commit) {
    (void)commit;
    gc_sram_flags = s_sram.flags;
    gc_sram_unlock_calls++;
}

u32 OSGetProgressiveMode(void) {
    OSSram *sram;
    u32 mode;

    sram = __OSLockSramHACK();
    mode = (sram->flags & 0x80u) >> 7;
    __OSUnlockSram(FALSE);
    return mode;
}

