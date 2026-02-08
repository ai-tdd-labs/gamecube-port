typedef signed short s16;
typedef unsigned int u32;
typedef unsigned char u8;

typedef struct { u8 _dummy; } GXFifoObj;
static GXFifoObj s_fifo_obj;

static u32 s_last_x;
static u32 s_last_y;

static void GXPosition2s16(s16 x, s16 y) {
    s_last_x = (u32)(signed int)x;
    s_last_y = (u32)(signed int)y;
}

static GXFifoObj *GXInit(void *base, u32 size) {
    (void)base; (void)size;
    s_last_x = 0;
    s_last_y = 0;
    return &s_fifo_obj;
}

int main(void) {
    static u8 fifo[0x20000];
    GXInit(fifo, sizeof(fifo));

    // MP4 wipe path uses s16 screen-space verts.
    GXPosition2s16((s16)-12, (s16)345);

    *(volatile u32 *)0x80300000 = 0xDEADBEEFu;
    *(volatile u32 *)0x80300004 = s_last_x;
    *(volatile u32 *)0x80300008 = s_last_y;
    while (1) {}
}
