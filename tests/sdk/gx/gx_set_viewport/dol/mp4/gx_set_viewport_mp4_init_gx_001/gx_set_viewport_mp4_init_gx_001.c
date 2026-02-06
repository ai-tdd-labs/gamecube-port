#include <stdint.h>
typedef uint32_t u32;
typedef uint8_t u8;

typedef struct { u8 _dummy; } GXFifoObj;
static GXFifoObj s_fifo_obj;

static u32 s_bp_sent_not;
static float s_vp_left, s_vp_top, s_vp_wd, s_vp_ht;

static GXFifoObj *GXInit(void *base, u32 size) {
    (void)base; (void)size;
    s_bp_sent_not = 0;
    return &s_fifo_obj;
}

static void GXSetViewportJitter(float left, float top, float wd, float ht, float nearz, float farz, u32 field) {
    (void)nearz; (void)farz;
    if (field == 0) top -= 0.5f;
    s_vp_left = left;
    s_vp_top = top;
    s_vp_wd = wd;
    s_vp_ht = ht;
    s_bp_sent_not = 1;
}

static void GXSetViewport(float left, float top, float wd, float ht, float nearz, float farz) {
    GXSetViewportJitter(left, top, wd, ht, nearz, farz, 1u);
}

int main(void) {
    static u8 fifo[0x20000];
    GXInit(fifo, sizeof(fifo));
    GXSetViewport(0.0f, 0.0f, 640.0f, 464.0f, 0.0f, 1.0f);

    *(volatile u32 *)0x80300000 = 0xDEADBEEFu;
    *(volatile u32 *)0x80300004 = s_bp_sent_not;
    *(volatile u32 *)0x80300008 = (u32)s_vp_left;
    *(volatile u32 *)0x8030000C = (u32)s_vp_top;
    *(volatile u32 *)0x80300010 = (u32)s_vp_wd;
    *(volatile u32 *)0x80300014 = (u32)s_vp_ht;
    while (1) {}
}
