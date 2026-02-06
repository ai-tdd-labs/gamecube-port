#include <stdint.h>

typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;

typedef struct { u8 _dummy; } GXFifoObj;
static GXFifoObj s_fifo_obj;

static u32 s_last;

static void GXPosition1x16(u16 x) {
    s_last = (u32)x;
}

static GXFifoObj *GXInit(void *base, u32 size) {
    (void)base; (void)size;
    s_last = 0;
    return &s_fifo_obj;
}

int main(void) {
    static u8 fifo[0x20000];
    GXInit(fifo, sizeof(fifo));

    GXPosition1x16((u16)0x1234);

    *(volatile u32 *)0x80300000 = 0xDEADBEEFu;
    *(volatile u32 *)0x80300004 = s_last;
    while (1) {}
}
