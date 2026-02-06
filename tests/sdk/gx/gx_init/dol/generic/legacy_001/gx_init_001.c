#ifdef GC_HOST_TEST
#include <stdint.h>
#include "runtime/runtime.h"
extern uint8_t *gc_test_ram;
#define RESULT_PTR() ((volatile uint8_t *)(gc_test_ram + 0x00300000))

typedef uint32_t u32;
typedef uint8_t u8;

typedef struct {
    u32 inDispList;
    u32 dlSaveContext;
    u32 genMode;
    u32 bpMask;
} GXData;

extern GXData gc_gx_data;

typedef struct {
    u8 _dummy;
} GXFifoObj;

GXFifoObj *GXInit(void *base, u32 size);

static inline void store_be32(volatile uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)((v >> 24) & 0xFF);
    p[1] = (uint8_t)((v >> 16) & 0xFF);
    p[2] = (uint8_t)((v >> 8) & 0xFF);
    p[3] = (uint8_t)(v & 0xFF);
}
#else
#include <stdint.h>
#define RESULT_PTR() ((volatile u32 *)0x80300000)

typedef struct {
    u32 inDispList;
    u32 dlSaveContext;
    u32 genMode;
    u32 bpMask;
} GXData;

typedef struct {
    u8 _dummy;
} GXFifoObj;

static GXData gx;
static GXFifoObj FifoObj;

void OSInit(void) {}

GXFifoObj *GXInit(void *base, u32 size) {
    (void)base;
    (void)size;
    gx.inDispList = 0;
    gx.dlSaveContext = 1;
    gx.genMode = 0;
    gx.bpMask = 0xFF;
    return &FifoObj;
}
#endif

int main(void) {
    static u8 fifo[0x10000] __attribute__((aligned(32)));
    GXFifoObj *ret;

#ifndef GC_HOST_TEST
    OSInit();
#endif

    ret = GXInit(fifo, sizeof(fifo));
    (void)ret;

#ifdef GC_HOST_TEST
    volatile uint8_t *result = RESULT_PTR();
    store_be32(result + 0x00, 0xDEADBEEF);
    store_be32(result + 0x04, 0x80000000u);
    store_be32(result + 0x08, gc_gx_data.inDispList);
    store_be32(result + 0x0C, gc_gx_data.dlSaveContext);
    store_be32(result + 0x10, gc_gx_data.genMode);
    store_be32(result + 0x14, gc_gx_data.bpMask);
#else
    volatile u32 *result = RESULT_PTR();
    result[0] = 0xDEADBEEF;
    result[1] = 0x80000000u;
    result[2] = gx.inDispList;
    result[3] = gx.dlSaveContext;
    result[4] = gx.genMode;
    result[5] = gx.bpMask;
    while (1) {}
#endif
    return 0;
}
