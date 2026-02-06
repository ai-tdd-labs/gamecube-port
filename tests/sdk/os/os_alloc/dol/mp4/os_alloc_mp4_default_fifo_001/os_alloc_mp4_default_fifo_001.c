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
#include <stdint.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
#define RESULT_PTR() ((volatile u32 *)0x80300000)

// Legacy synthetic implementation: simple bump allocator.
static u8 *s_heap_cur;
static u8 *s_heap_end;

static void OSInitAlloc(void *lo, void *hi, int max_heaps) {
    (void)max_heaps;
    s_heap_cur = (u8 *)lo;
    s_heap_end = (u8 *)hi;
}

static int OSCreateHeap(void *lo, void *hi) {
    s_heap_cur = (u8 *)lo;
    s_heap_end = (u8 *)hi;
    return 0;
}

static void OSSetCurrentHeap(int heap) { (void)heap; }

static void *OSAlloc(u32 size) {
    // This legacy DOL testcase wants to mirror the SDK's "cell header" behavior:
    // OSAlloc returns (cell + 0x20). We model that by bumping a cell pointer and
    // returning cell+0x20, where cell size is (size+0x20) rounded to 32B.
    u32 req = (size + 0x20u + 31u) & ~31u;
    if (s_heap_cur + req > s_heap_end) {
        return (void *)0;
    }
    u8 *cell = s_heap_cur;
    s_heap_cur += req;
    return (void *)(cell + 0x20u);
}
#endif

int main(void) {
    OSInitAlloc((void*)0x80002000, (void*)0x80200000, 2);
    int heap = OSCreateHeap((void*)0x80003000, (void*)0x80200000);
    OSSetCurrentHeap(heap);

    void *p1 = OSAlloc(0x100000);
    void *p2 = (void*)0;// unused


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
