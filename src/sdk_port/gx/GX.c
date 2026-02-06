#include <stdint.h>

typedef uint32_t u32;

typedef struct {
    u32 inDispList;
    u32 dlSaveContext;
    u32 genMode;
    u32 bpMask;
} GXData;

GXData gc_gx_data;

typedef struct {
    uint8_t _dummy;
} GXFifoObj;

static GXFifoObj s_fifo_obj;

GXFifoObj *GXInit(void *base, u32 size) {
    (void)base;
    (void)size;
    gc_gx_data.inDispList = 0;
    gc_gx_data.dlSaveContext = 1;
    gc_gx_data.genMode = 0;
    gc_gx_data.bpMask = 0xFF;
    return &s_fifo_obj;
}

