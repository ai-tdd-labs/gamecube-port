#include <stdint.h>

typedef uint16_t u16;
typedef uint32_t u32;

// RAM-backed state (big-endian in MEM1) for dump comparability.
#include "../sdk_state.h"

// OS interrupt primitives (sdk_port model).
int OSDisableInterrupts(void);
int OSRestoreInterrupts(int level);

u16 gc_vi_regs[64];
u32 gc_vi_disable_calls;
u32 gc_vi_restore_calls;
u32 gc_vi_flush_calls;
u32 gc_vi_wait_retrace_calls;
u32 gc_vi_get_next_field_calls;
u32 gc_vi_get_retrace_count_calls;
u32 gc_vi_set_black_calls;
u32 gc_vi_next_field;
u32 gc_vi_retrace_count;
u32 gc_vi_black;
u32 gc_vi_post_cb_calls;
u32 gc_vi_post_cb_last_arg;

// VISetNextFrameBuffer is used very early by games (including MP4). For now our
// deterministic tests only require that the symbol exists.
void VISetNextFrameBuffer(void *fb) { (void)fb; }

typedef void (*VIRetraceCallback)(u32 retraceCount);

static u32 gc_vi_post_cb_ptr;
static VIRetraceCallback gc_vi_post_cb_fn;

static int VI_DisableInterrupts(void) {
    u32 v = gc_sdk_state_load_u32_or(GC_SDK_OFF_VI_DISABLE_CALLS, gc_vi_disable_calls);
    v++;
    gc_sdk_state_store_u32_mirror(GC_SDK_OFF_VI_DISABLE_CALLS, &gc_vi_disable_calls, v);
    return OSDisableInterrupts();
}

static void VI_RestoreInterrupts(int level) {
    u32 v = gc_sdk_state_load_u32_or(GC_SDK_OFF_VI_RESTORE_CALLS, gc_vi_restore_calls);
    v++;
    gc_sdk_state_store_u32_mirror(GC_SDK_OFF_VI_RESTORE_CALLS, &gc_vi_restore_calls, v);
    (void)OSRestoreInterrupts(level);
}

VIRetraceCallback VISetPostRetraceCallback(VIRetraceCallback callback) {
    u32 old = gc_sdk_state_load_u32_or(GC_SDK_OFF_VI_POST_CB_PTR, gc_vi_post_cb_ptr);
    VIRetraceCallback old_fn = gc_vi_post_cb_fn;

    // Match SDK pattern: protect callback swap with interrupts disabled.
    int lvl = VI_DisableInterrupts();
    gc_vi_post_cb_fn = callback;
    gc_vi_post_cb_ptr = (u32)(uintptr_t)callback;
    gc_sdk_state_store_u32_mirror(GC_SDK_OFF_VI_POST_CB_PTR, &gc_vi_post_cb_ptr, gc_vi_post_cb_ptr);
    u32 calls = gc_sdk_state_load_u32_or(GC_SDK_OFF_VI_POST_CB_SET_CALLS, 0);
    calls++;
    gc_sdk_state_store_u32_mirror(GC_SDK_OFF_VI_POST_CB_SET_CALLS, 0, calls);
    VI_RestoreInterrupts(lvl);

    // Return the previous callback (host-safe; doesn't truncate pointers).
    // For our deterministic dumps we still store the pointer token in sdk_state.
    (void)old;
    return old_fn;
}

void VIFlush(void) {
    u32 v = gc_sdk_state_load_u32_or(GC_SDK_OFF_VI_FLUSH_CALLS, gc_vi_flush_calls);
    v++;
    gc_sdk_state_store_u32_mirror(GC_SDK_OFF_VI_FLUSH_CALLS, &gc_vi_flush_calls, v);
}

void VIWaitForRetrace(void) {
    u32 v = gc_sdk_state_load_u32_or(GC_SDK_OFF_VI_WAIT_RETRACE_CALLS, gc_vi_wait_retrace_calls);
    v++;
    gc_sdk_state_store_u32_mirror(GC_SDK_OFF_VI_WAIT_RETRACE_CALLS, &gc_vi_wait_retrace_calls, v);

    // We do not emulate the actual VI interrupt machinery on host.
    // Instead, model the observable consequence: retraceCount advances and
    // PostRetraceCallback is invoked once per retrace.
    u32 rc = gc_sdk_state_load_u32_or(GC_SDK_OFF_VI_RETRACE_COUNT, gc_vi_retrace_count);
    rc++;
    gc_sdk_state_store_u32_mirror(GC_SDK_OFF_VI_RETRACE_COUNT, &gc_vi_retrace_count, rc);

    if (gc_vi_post_cb_fn) {
        gc_vi_post_cb_fn(rc);
        u32 calls = gc_sdk_state_load_u32_or(GC_SDK_OFF_VI_POST_CB_CALLS, gc_vi_post_cb_calls);
        calls++;
        gc_sdk_state_store_u32_mirror(GC_SDK_OFF_VI_POST_CB_CALLS, &gc_vi_post_cb_calls, calls);
        gc_sdk_state_store_u32_mirror(GC_SDK_OFF_VI_POST_CB_LAST_ARG, &gc_vi_post_cb_last_arg, rc);
    }
}

#define VI_DTV_STAT 55

u32 VIGetDTVStatus(void) {
    u32 stat;
    int interrupt;

    interrupt = VI_DisableInterrupts();
    // regs are u16 BE in RAM-backed state.
    u16 r = gc_sdk_state_load_u16be_or(GC_SDK_OFF_VI_REGS_U16BE + (VI_DTV_STAT * 2u), gc_vi_regs[VI_DTV_STAT]);
    gc_vi_regs[VI_DTV_STAT] = r;
    stat = ((u32)r & 3u);
    VI_RestoreInterrupts(interrupt);
    return (stat & 1u);
}

// Minimal VIInit/VIGetTvFormat for deterministic tests.
static u32 s_tv_format;
enum { VI_NTSC = 0 };

void VIInit(void) {
    s_tv_format = VI_NTSC;
    gc_sdk_state_store_u32_mirror(GC_SDK_OFF_VI_TV_FORMAT, &s_tv_format, s_tv_format);
}
u32 VIGetTvFormat(void) {
    s_tv_format = gc_sdk_state_load_u32_or(GC_SDK_OFF_VI_TV_FORMAT, s_tv_format);
    return s_tv_format;
}

u32 VIGetNextField(void) {
    u32 c = gc_sdk_state_load_u32_or(GC_SDK_OFF_VI_GET_NEXT_FIELD_CALLS, gc_vi_get_next_field_calls);
    c++;
    gc_sdk_state_store_u32_mirror(GC_SDK_OFF_VI_GET_NEXT_FIELD_CALLS, &gc_vi_get_next_field_calls, c);
    gc_vi_next_field = gc_sdk_state_load_u32_or(GC_SDK_OFF_VI_NEXT_FIELD, gc_vi_next_field);
    return gc_vi_next_field;
}

u32 VIGetRetraceCount(void) {
    u32 c = gc_sdk_state_load_u32_or(GC_SDK_OFF_VI_GET_RETRACE_COUNT_CALLS, gc_vi_get_retrace_count_calls);
    c++;
    gc_sdk_state_store_u32_mirror(GC_SDK_OFF_VI_GET_RETRACE_COUNT_CALLS, &gc_vi_get_retrace_count_calls, c);
    gc_vi_retrace_count = gc_sdk_state_load_u32_or(GC_SDK_OFF_VI_RETRACE_COUNT, gc_vi_retrace_count);
    return gc_vi_retrace_count;
}

void VISetBlack(u32 black) {
    u32 c = gc_sdk_state_load_u32_or(GC_SDK_OFF_VI_SET_BLACK_CALLS, gc_vi_set_black_calls);
    c++;
    gc_sdk_state_store_u32_mirror(GC_SDK_OFF_VI_SET_BLACK_CALLS, &gc_vi_set_black_calls, c);
    u32 v = black ? 1u : 0u;
    gc_sdk_state_store_u32_mirror(GC_SDK_OFF_VI_BLACK, &gc_vi_black, v);
}

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

    u32 non_inter = gc_sdk_state_load_u32_or(GC_SDK_OFF_VI_NON_INTER, gc_vi_non_inter);
    u32 change_mode = gc_sdk_state_load_u32_or(GC_SDK_OFF_VI_CHANGE_MODE, gc_vi_change_mode);

    u32 new_non_inter = rm->viTVmode & 3u;
    if (non_inter != new_non_inter) {
        change_mode = 1;
        non_inter = new_non_inter;
    }
    gc_sdk_state_store_u32_mirror(GC_SDK_OFF_VI_CHANGE_MODE, &gc_vi_change_mode, change_mode);
    gc_sdk_state_store_u32_mirror(GC_SDK_OFF_VI_NON_INTER, &gc_vi_non_inter, non_inter);

    gc_sdk_state_store_u32_mirror(GC_SDK_OFF_VI_DISP_POS_X, &gc_vi_disp_pos_x, (u32)rm->viXOrigin);
    gc_sdk_state_store_u32_mirror(GC_SDK_OFF_VI_DISP_POS_Y, &gc_vi_disp_pos_y,
                    (non_inter == 1u) ? (u32)(rm->viYOrigin * 2u) : (u32)rm->viYOrigin);
    gc_sdk_state_store_u32_mirror(GC_SDK_OFF_VI_DISP_SIZE_X, &gc_vi_disp_size_x, (u32)rm->viWidth);
    gc_sdk_state_store_u32_mirror(GC_SDK_OFF_VI_FB_SIZE_X, &gc_vi_fb_size_x, (u32)rm->fbWidth);
    gc_sdk_state_store_u32_mirror(GC_SDK_OFF_VI_FB_SIZE_Y, &gc_vi_fb_size_y, (u32)rm->xfbHeight);
    gc_sdk_state_store_u32_mirror(GC_SDK_OFF_VI_XFB_MODE, &gc_vi_xfb_mode, (u32)rm->xFBmode);

    // helper() is called 7 times in the legacy testcase.
    u32 helpers = gc_sdk_state_load_u32_or(GC_SDK_OFF_VI_HELPER_CALLS, gc_vi_helper_calls);
    helpers += 7u;
    gc_sdk_state_store_u32_mirror(GC_SDK_OFF_VI_HELPER_CALLS, &gc_vi_helper_calls, helpers);

    OSRestoreInterrupts(1);
}

u32 gc_vi_pan_pos_x;
u32 gc_vi_pan_pos_y;
u32 gc_vi_pan_size_x;
u32 gc_vi_pan_size_y;

void VIConfigurePan(u16 xOrg, u16 yOrg, u16 width, u16 height) {
    (void)OSDisableInterrupts();
    gc_sdk_state_store_u32_mirror(GC_SDK_OFF_VI_PAN_POS_X, &gc_vi_pan_pos_x, (u32)xOrg);
    gc_sdk_state_store_u32_mirror(GC_SDK_OFF_VI_PAN_POS_Y, &gc_vi_pan_pos_y, (u32)yOrg);
    gc_sdk_state_store_u32_mirror(GC_SDK_OFF_VI_PAN_SIZE_X, &gc_vi_pan_size_x, (u32)width);
    gc_sdk_state_store_u32_mirror(GC_SDK_OFF_VI_PAN_SIZE_Y, &gc_vi_pan_size_y, (u32)height);

    // Mirror legacy testcase formula.
    const u32 VI_PROGRESSIVE = 2u;
    const u32 VI_3D = 3u;
    const u32 VI_XFBMODE_SF = 1u;
    u32 non_inter = gc_sdk_state_load_u32_or(GC_SDK_OFF_VI_NON_INTER, gc_vi_non_inter);
    u32 xfb_mode = gc_sdk_state_load_u32_or(GC_SDK_OFF_VI_XFB_MODE, gc_vi_xfb_mode);
    u32 pan_size_y = gc_sdk_state_load_u32_or(GC_SDK_OFF_VI_PAN_SIZE_Y, gc_vi_pan_size_y);
    if (non_inter == VI_PROGRESSIVE || non_inter == VI_3D) {
        gc_sdk_state_store_u32_mirror(GC_SDK_OFF_VI_DISP_SIZE_Y, &gc_vi_disp_size_y, pan_size_y);
    } else {
        u32 v = (xfb_mode == VI_XFBMODE_SF) ? (pan_size_y * 2u) : pan_size_y;
        gc_sdk_state_store_u32_mirror(GC_SDK_OFF_VI_DISP_SIZE_Y, &gc_vi_disp_size_y, v);
    }

    // helper() is called 5 times in the legacy testcase.
    u32 helpers = gc_sdk_state_load_u32_or(GC_SDK_OFF_VI_HELPER_CALLS, gc_vi_helper_calls);
    helpers += 5u;
    gc_sdk_state_store_u32_mirror(GC_SDK_OFF_VI_HELPER_CALLS, &gc_vi_helper_calls, helpers);
    OSRestoreInterrupts(1);
}
