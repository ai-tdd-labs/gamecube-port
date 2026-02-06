#include <stdint.h>

typedef uint32_t u32;
typedef uint8_t u8;

typedef struct { u8 _dummy; } GXFifoObj;
static GXFifoObj s_fifo_obj;

static u32 s_clip_mode;
static u32 s_bp_sent_not;

static GXFifoObj *GXInit(void *base, u32 size) {
    (void)base; (void)size;
    s_clip_mode = 0xFFFFFFFFu;
    s_bp_sent_not = 0;
    return &s_fifo_obj;
}

static void GXSetClipMode(u32 mode) {
    s_clip_mode = mode;
    s_bp_sent_not = 1;
}

enum { GX_CLIP_ENABLE = 0 };

int main(void) {
    static u8 fifo[0x20000];
    GXInit(fifo, sizeof(fifo));

    GXSetClipMode(GX_CLIP_ENABLE);

    *(volatile u32 *)0x80300000 = 0xDEADBEEFu;
    *(volatile u32 *)0x80300004 = s_clip_mode;
    *(volatile u32 *)0x80300008 = s_bp_sent_not;
    while (1) {}
}
