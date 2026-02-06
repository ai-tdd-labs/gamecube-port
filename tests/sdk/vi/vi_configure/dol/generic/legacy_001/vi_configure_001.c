#ifdef GC_HOST_TEST
#include <stdint.h>
#include "runtime/vi.h"
extern uint8_t *gc_test_ram;
extern uint32_t gc_vi_change_mode;
extern uint32_t gc_vi_disp_pos_x;
extern uint32_t gc_vi_disp_pos_y;
extern uint32_t gc_vi_disp_size_x;
extern uint32_t gc_vi_fb_size_x;
extern uint32_t gc_vi_fb_size_y;
extern uint32_t gc_vi_xfb_mode;
extern uint32_t gc_vi_helper_calls;
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

#define VI_NTSC 0
#define VI_DEBUG_PAL 4
#define VI_NON_INTERLACE 1
#define VI_PROGRESSIVE 2
#define VI_3D 3
#define VI_XFBMODE_SF 1

typedef struct {
    u32 viTVmode;
    u16 fbWidth;
    u16 efbHeight;
    u16 xfbHeight;
    u16 viXOrigin;
    u16 viYOrigin;
    u16 viWidth;
    u16 viHeight;
    u32 xFBmode;
} GXRenderModeObj;

typedef struct {
    u16 dispPosX;
    u16 dispPosY;
    u16 dispSizeX;
    u16 dispSizeY;
    u16 fbSizeX;
    u16 fbSizeY;
    u16 panSizeX;
    u16 panSizeY;
    u16 panPosX;
    u16 panPosY;
    u8 nonInter;
    u8 tv;
    u8 xfbMode;
    u8 is3D;
} HorVerInfo;

static HorVerInfo HorVer;
static u32 changeMode = 0;
static u32 helper_calls = 0;

void OSInit(void) {}

int OSDisableInterrupts(void) { return 1; }
void OSRestoreInterrupts(int enabled) { (void)enabled; }

static void helper(void) { helper_calls++; }

void VIConfigure(const GXRenderModeObj *obj) {
    u32 newNonInter;

    OSDisableInterrupts();
    newNonInter = obj->viTVmode & 3;

    if (HorVer.nonInter != newNonInter) {
        changeMode = 1;
        HorVer.nonInter = (u8)newNonInter;
    }

    HorVer.tv = (u8)(obj->viTVmode >> 2);

    HorVer.dispPosX = obj->viXOrigin;
    HorVer.dispPosY = (u16)((HorVer.nonInter == VI_NON_INTERLACE) ? (obj->viYOrigin * 2) : obj->viYOrigin);
    HorVer.dispSizeX = obj->viWidth;
    HorVer.fbSizeX = obj->fbWidth;
    HorVer.fbSizeY = obj->xfbHeight;
    HorVer.xfbMode = (u8)obj->xFBmode;
    HorVer.panSizeX = HorVer.fbSizeX;
    HorVer.panSizeY = HorVer.fbSizeY;
    HorVer.panPosX = 0;
    HorVer.panPosY = 0;
    HorVer.dispSizeY = (u16)((HorVer.nonInter == VI_PROGRESSIVE || HorVer.nonInter == VI_3D)
        ? HorVer.panSizeY
        : (HorVer.xfbMode == VI_XFBMODE_SF) ? (u16)(2 * HorVer.panSizeY) : HorVer.panSizeY);
    HorVer.is3D = (HorVer.nonInter == VI_3D) ? 1 : 0;

    helper();
    helper();
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
    GXRenderModeObj obj;
    OSInit();
    HorVer.nonInter = 0;
    changeMode = 0;
    helper_calls = 0;

    obj.viTVmode = (VI_NTSC << 2) | VI_NON_INTERLACE;
    obj.viXOrigin = 10;
    obj.viYOrigin = 20;
    obj.viWidth = 320;
    obj.fbWidth = 320;
    obj.xfbHeight = 240;
    obj.xFBmode = VI_XFBMODE_SF;

    VIConfigure(&obj);
#endif

#ifdef GC_HOST_TEST
    GXRenderModeObj obj;
    gc_vi_change_mode = 0;
    gc_vi_helper_calls = 0;
    obj.viTVmode = (VI_NTSC << 2) | VI_NON_INTERLACE;
    obj.viXOrigin = 10;
    obj.viYOrigin = 20;
    obj.viWidth = 320;
    obj.fbWidth = 320;
    obj.xfbHeight = 240;
    obj.xFBmode = VI_XFBMODE_SF;
    VIConfigure(&obj);
#endif

#ifdef GC_HOST_TEST
    volatile uint8_t *result = RESULT_PTR();
    store_be32(result + 0x00, 0xDEADBEEF);
    store_be32(result + 0x04, gc_vi_change_mode);
    store_be32(result + 0x08, gc_vi_disp_pos_x);
    store_be32(result + 0x0C, gc_vi_disp_pos_y);
    store_be32(result + 0x10, gc_vi_disp_size_x);
    store_be32(result + 0x14, gc_vi_fb_size_x);
    store_be32(result + 0x18, gc_vi_fb_size_y);
    store_be32(result + 0x1C, gc_vi_xfb_mode);
    store_be32(result + 0x20, gc_vi_helper_calls);
#else
    volatile u32 *result = RESULT_PTR();
    result[0] = 0xDEADBEEF;
    result[1] = changeMode;
    result[2] = HorVer.dispPosX;
    result[3] = HorVer.dispPosY;
    result[4] = HorVer.dispSizeX;
    result[5] = HorVer.fbSizeX;
    result[6] = HorVer.fbSizeY;
    result[7] = HorVer.xfbMode;
    result[8] = helper_calls;
    while (1) {}
#endif
    return 0;
}
