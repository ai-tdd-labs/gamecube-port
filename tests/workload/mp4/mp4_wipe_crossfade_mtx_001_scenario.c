#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

// This scenario is a host workload reachability checkpoint.
// It forces the wipe system down the WipeCrossFade() path (type=CROSS, mode=IN),
// and opts into the host-safe texture-copy subset via:
//   GC_HOST_WORKLOAD_MTX=1
//   GC_HOST_WORKLOAD_WIPE_CROSSFADE=1

#include "dolphin/gx.h"

typedef uint32_t u32;
typedef uint8_t u8;
typedef uint16_t u16;

void WipeInit(GXRenderModeObj *rmode);
void WipeExecAlways(void);

// Mirror WipeState layout from tests/workload/mp4/slices/wipeexecalways_decomp_blank.c.
// (Workload reachability only; this is not a correctness oracle.)
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

// Constants match tests/workload/mp4/slices/wipeexecalways_decomp_blank.c.
enum {
    WIPE_TYPE_CROSS = 1,
    WIPE_MODE_IN = 1,
};

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

const char *gc_scenario_label(void) { return "workload/mp4_wipe_crossfade_mtx_001"; }
const char *gc_scenario_out_path(void) { return "../../actual/workload/mp4_wipe_crossfade_mtx_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    (void)ram;

    WipeInit(&GXNtsc480IntDf);

    // Force crossfade path and allocation/copy setup on first call.
    wipeData.type = (u8)WIPE_TYPE_CROSS;
    wipeData.mode = (u8)WIPE_MODE_IN;
    wipeData.time = 0.0f;
    wipeData.duration = 10.0f;
    wipeData.copy_data = 0;
    wipeData.keep_copy = 1;

    WipeExecAlways();

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x10);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0x00, 0x58434641u); // "XCFA"
    wr32be(p + 0x04, 0xDEADBEEFu);
    wr32be(p + 0x08, (u32)(uintptr_t)wipeData.copy_data);
    wr32be(p + 0x0C, 0);
}

