#ifdef GC_HOST_TEST
#include <stdint.h>
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
#include <dolphin/os/OSAlloc.h>
#include <dolphin/types.h>
#define RESULT_PTR() ((volatile u32 *)0x80300000)
#endif

int main(void) {
    OSInitAlloc((void*)0x80002000, (void*)0x80008000, 2);
    int heap = OSCreateHeap((void*)0x80003000, (void*)0x80007000);
    OSSetCurrentHeap(heap);

    void *p1 = OSAlloc(0x20);
    void *p2 = OSAlloc(0x40);

#ifdef GC_HOST_TEST
    volatile uint8_t *result = RESULT_PTR();
    store_be32(result + 0, 0xDEADBEEF);
    store_be32(result + 4, (uint32_t)p1);
    store_be32(result + 8, (uint32_t)p2);
#else
    volatile u32 *result = RESULT_PTR();
    result[0] = 0xDEADBEEF;
    result[1] = (u32)p1;
    result[2] = (u32)p2;
    while (1) {
    }
#endif
    return 0;
}
