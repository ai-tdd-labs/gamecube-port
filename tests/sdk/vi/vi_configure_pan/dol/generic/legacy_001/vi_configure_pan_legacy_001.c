#ifdef GC_HOST_TEST
#include <stdint.h>
#include "runtime/vi.h"
extern uint8_t *gc_test_ram;
extern uint32_t gc_vi_pan_pos_x;
extern uint32_t gc_vi_pan_pos_y;
extern uint32_t gc_vi_pan_size_x;
extern uint32_t gc_vi_pan_size_y;
extern uint32_t gc_vi_disp_size_y;
extern uint32_t gc_vi_helper_calls;
extern uint32_t gc_vi_non_inter;
extern uint32_t gc_vi_xfb_mode;
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

#define VI_NON_INTERLACE 1
#define VI_PROGRESSIVE 2
#define VI_3D 3
#define VI_XFBMODE_SF 1

typedef struct {
    u16 panPosX;
    u16 panPosY;
    u16 panSizeX;
    u16 panSizeY;
    u16 dispSizeY;
    u8 nonInter;
    u8 xfbMode;
    void *timing;
} HorVerInfo;

static HorVerInfo HorVer;
static u32 helper_calls = 0;

void OSInit(void) {}

int OSDisableInterrupts(void) { return 1; }
void OSRestoreInterrupts(int enabled) { (void)enabled; }

static void helper(void) { helper_calls++; }

void VIConfigurePan(u16 xOrg, u16 yOrg, u16 width, u16 height) {
    OSDisableInterrupts();
    HorVer.panPosX = xOrg;
    HorVer.panPosY = yOrg;
    HorVer.panSizeX = width;
    HorVer.panSizeY = height;
    HorVer.dispSizeY = (HorVer.nonInter == VI_PROGRESSIVE || HorVer.nonInter == VI_3D)
        ? HorVer.panSizeY
        : (HorVer.xfbMode == VI_XFBMODE_SF) ? (u16)(HorVer.panSizeY * 2) : HorVer.panSizeY;
    helper();
    helper();
    helper();
    helper();
    helper();
    OSRestoreInterrupts(1);
}
#endif

int main(void) {
#ifndef GC_HOST_TEST
    OSInit();
    HorVer.nonInter = VI_NON_INTERLACE;
    HorVer.xfbMode = VI_XFBMODE_SF;
    helper_calls = 0;
    VIConfigurePan(10, 20, 320, 240);
#endif

#ifdef GC_HOST_TEST
    gc_vi_non_inter = VI_NON_INTERLACE;
    gc_vi_xfb_mode = VI_XFBMODE_SF;
    gc_vi_helper_calls = 0;
    VIConfigurePan(10, 20, 320, 240);
    volatile uint8_t *result = RESULT_PTR();
    store_be32(result + 0x00, 0xDEADBEEF);
    store_be32(result + 0x04, gc_vi_pan_pos_x);
    store_be32(result + 0x08, gc_vi_pan_pos_y);
    store_be32(result + 0x0C, gc_vi_pan_size_x);
    store_be32(result + 0x10, gc_vi_pan_size_y);
    store_be32(result + 0x14, gc_vi_disp_size_y);
    store_be32(result + 0x18, gc_vi_helper_calls);
#else
    volatile u32 *result = RESULT_PTR();
    result[0] = 0xDEADBEEF;
    result[1] = HorVer.panPosX;
    result[2] = HorVer.panPosY;
    result[3] = HorVer.panSizeX;
    result[4] = HorVer.panSizeY;
    result[5] = HorVer.dispSizeY;
    result[6] = helper_calls;
    while (1) {}
#endif
    return 0;
}
