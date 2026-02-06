#include <stdint.h>

typedef uint16_t u16;
typedef uint32_t u32;

u16 gc_vi_regs[64];
u32 gc_vi_disable_calls;
u32 gc_vi_restore_calls;
u32 gc_vi_flush_calls;
u32 gc_vi_wait_retrace_calls;

// VISetNextFrameBuffer is used very early by games (including MP4). For now our
// deterministic tests only require that the symbol exists.
void VISetNextFrameBuffer(void *fb) { (void)fb; }

void VIFlush(void) { gc_vi_flush_calls++; }

void VIWaitForRetrace(void) { gc_vi_wait_retrace_calls++; }

static int OSDisableInterrupts(void) {
    gc_vi_disable_calls++;
    return 1;
}

static void OSRestoreInterrupts(int enabled) {
    (void)enabled;
    gc_vi_restore_calls++;
}

#define VI_DTV_STAT 55

u32 VIGetDTVStatus(void) {
    u32 stat;
    int interrupt;

    interrupt = OSDisableInterrupts();
    stat = (gc_vi_regs[VI_DTV_STAT] & 3u);
    OSRestoreInterrupts(interrupt);
    return (stat & 1u);
}

// Minimal VIInit/VIGetTvFormat for deterministic tests.
static u32 s_tv_format;
#define VI_NTSC 0

void VIInit(void) { s_tv_format = VI_NTSC; }
u32 VIGetTvFormat(void) { return s_tv_format; }

// Minimal VIConfigure/VIConfigurePan:
// Implement the observable side effects used by our deterministic tests.
u32 gc_vi_change_mode;
u32 gc_vi_disp_pos_x;
u32 gc_vi_disp_pos_y;
u32 gc_vi_disp_size_x;
u32 gc_vi_disp_size_y;
u32 gc_vi_fb_size_x;
u32 gc_vi_fb_size_y;
u32 gc_vi_xfb_mode;
u32 gc_vi_helper_calls;
u32 gc_vi_non_inter;

void VIConfigure(const void *obj) {
    // This mirrors the logic embedded in the legacy DOL testcase. It is not a
    // complete SDK implementation; we only model fields that the tests assert.
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

    const GXRenderModeObj *rm = (const GXRenderModeObj *)obj;
    if (!rm) return;

    (void)OSDisableInterrupts();

    u32 new_non_inter = rm->viTVmode & 3u;
    if (gc_vi_non_inter != new_non_inter) {
        gc_vi_change_mode = 1;
        gc_vi_non_inter = new_non_inter;
    }

    gc_vi_disp_pos_x = rm->viXOrigin;
    gc_vi_disp_pos_y = (gc_vi_non_inter == 1u) ? (u32)(rm->viYOrigin * 2u) : (u32)rm->viYOrigin;
    gc_vi_disp_size_x = rm->viWidth;
    gc_vi_fb_size_x = rm->fbWidth;
    gc_vi_fb_size_y = rm->xfbHeight;
    gc_vi_xfb_mode = rm->xFBmode;

    // helper() is called 7 times in the legacy testcase.
    for (int i = 0; i < 7; i++) gc_vi_helper_calls++;

    OSRestoreInterrupts(1);
}

u32 gc_vi_pan_pos_x;
u32 gc_vi_pan_pos_y;
u32 gc_vi_pan_size_x;
u32 gc_vi_pan_size_y;

void VIConfigurePan(u16 xOrg, u16 yOrg, u16 width, u16 height) {
    (void)OSDisableInterrupts();
    gc_vi_pan_pos_x = xOrg;
    gc_vi_pan_pos_y = yOrg;
    gc_vi_pan_size_x = width;
    gc_vi_pan_size_y = height;

    // Mirror legacy testcase formula.
    const u32 VI_PROGRESSIVE = 2u;
    const u32 VI_3D = 3u;
    const u32 VI_XFBMODE_SF = 1u;
    if (gc_vi_non_inter == VI_PROGRESSIVE || gc_vi_non_inter == VI_3D) {
        gc_vi_disp_size_y = gc_vi_pan_size_y;
    } else {
        gc_vi_disp_size_y = (gc_vi_xfb_mode == VI_XFBMODE_SF) ? (gc_vi_pan_size_y * 2u) : gc_vi_pan_size_y;
    }

    // helper() is called 5 times in the legacy testcase.
    for (int i = 0; i < 5; i++) gc_vi_helper_calls++;
    OSRestoreInterrupts(1);
}
