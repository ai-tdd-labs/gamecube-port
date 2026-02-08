#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

#include "sdk_state.h"

typedef uint32_t u32;

typedef void (*VIRetraceCallback)(u32 retraceCount);
VIRetraceCallback VISetPostRetraceCallback(VIRetraceCallback cb);
void VIWaitForRetrace(void);

static u32 s_cb_calls;
static u32 s_cb_last;

static void PostCB(u32 retrace) {
    s_cb_calls++;
    s_cb_last = retrace;
}

const char *gc_scenario_label(void) { return "VIWaitForRetrace/mp4_calls_post_cb_hupadinit_001"; }
const char *gc_scenario_out_path(void) { return "../actual/vi_wait_for_retrace_calls_post_cb_hupadinit_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    (void)VISetPostRetraceCallback(PostCB);
    VIWaitForRetrace();

    uint8_t *out = gc_ram_ptr(ram, 0x80300000u, 0x40);
    if (!out) die("gc_ram_ptr failed");
    wr32be(out + 0x00, 0xDEADBEEFu);
    wr32be(out + 0x04, s_cb_calls);
    wr32be(out + 0x08, s_cb_last);
    wr32be(out + 0x0C, gc_sdk_state_load_u32be(GC_SDK_OFF_VI_POST_CB_CALLS));
    wr32be(out + 0x10, gc_sdk_state_load_u32be(GC_SDK_OFF_VI_POST_CB_LAST_ARG));
    wr32be(out + 0x14, gc_sdk_state_load_u32be(GC_SDK_OFF_VI_RETRACE_COUNT));
}
