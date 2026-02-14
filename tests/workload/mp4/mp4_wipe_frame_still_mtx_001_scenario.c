#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

// This scenario is a host workload reachability checkpoint.
// It forces the wipe system down the "copy_data != NULL" path so WipeFrameStill()
// can run its GX+MTX state setup (when GC_HOST_WORKLOAD_MTX=1 is enabled).

#include "dolphin/gx.h"

typedef uint32_t u32;

void WipeInit(GXRenderModeObj *rmode);
void WipeExecAlways(void);

// Mirror WipeState layout from tests/workload/mp4/slices/wipeexecalways_decomp_blank.c.
// (Workload reachability only; this is not a correctness oracle.)
typedef uint8_t u8;
typedef uint16_t u16;

typedef struct wipe_state {
    u32 unk00;
    u32 unk04;
    void *copy_data;
    u32 unk0C;
    void *unk10[8];
    float time;
    float duration;
    u32 unk38;
    u16 w;
    u16 h;
    u16 x;
    u16 y;
    GXColor color;
    volatile u8 type;
    u8 mode;
    u8 stat;
    u8 keep_copy;
} WipeState;

extern WipeState wipeData;

// Minimal render modes required by WipeInit.
GXRenderModeObj GXNtsc480IntDf = {
    .viTVmode = 0,
    .fbWidth = 640,
    .efbHeight = 480,
    .xfbHeight = 480,
    .aa = 0,
    .field_rendering = 0,
    .sample_pattern = {0},
    .vfilter = {0},
};
GXRenderModeObj GXNtsc480Prog = {
    .viTVmode = 0,
    .fbWidth = 640,
    .efbHeight = 480,
    .xfbHeight = 480,
    .aa = 0,
    .field_rendering = 0,
    .sample_pattern = {0},
    .vfilter = {0},
};
GXRenderModeObj GXPal528IntDf = {
    .viTVmode = 1,
    .fbWidth = 640,
    .efbHeight = 528,
    .xfbHeight = 528,
    .aa = 0,
    .field_rendering = 0,
    .sample_pattern = {0},
    .vfilter = {0},
};
GXRenderModeObj GXMpal480IntDf = {
    .viTVmode = 2,
    .fbWidth = 640,
    .efbHeight = 480,
    .xfbHeight = 480,
    .aa = 0,
    .field_rendering = 0,
    .sample_pattern = {0},
    .vfilter = {0},
};

const char *gc_scenario_label(void) { return "workload/mp4_wipe_frame_still_mtx_001"; }
const char *gc_scenario_out_path(void) { return "../../actual/workload/mp4_wipe_frame_still_mtx_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    (void)ram;

    WipeInit(&GXNtsc480IntDf);

    // Force the branch in WipeExecAlways that calls WipeFrameStill().
    // We don't model the texture copy; we only need a non-null pointer.
    wipeData.copy_data = (void *)(uintptr_t)0x80010000u;

    WipeExecAlways();

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x10);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0x00, 0x57495045u); // "WIPE"
    wr32be(p + 0x04, 0xDEADBEEFu);
    wr32be(p + 0x08, (u32)(uintptr_t)wipeData.copy_data);
    wr32be(p + 0x0C, 0);
}
