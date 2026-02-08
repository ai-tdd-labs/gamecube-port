#include <stdint.h>

// Host harness helpers.
#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

// Decomp headers (via tools/run_host_scenario.sh workload include paths)
#include "dolphin/gx.h"

// MP4 decomp/workload functions (compiled in via extra_srcs in tools/run_host_scenario.sh)
void HuSysInit(GXRenderModeObj *mode);
void HuPrcInit(void);
void HuPadInit(void);
void GWInit(void);
void pfInit(void);
void HuSprInit(void);

// Host-only stubs (tests/workload/mp4/slices/post_sprinit_stubs.c)
void Hu3DInit(void);
void HuDataInit(void);
void HuPerfInit(void);
int HuPerfCreate(const char *name, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void WipeInit(void *render_mode);
void omMasterInit(int a0, void *ovltbl, int ovl_count, int ovl_boot);

// Nintendo SDK
void VIWaitForRetrace(void);

// ---- Game-specific externs referenced by MP4 init/pad/process/gwinit ----
// Minimal render modes required by HuSysInit/InitRenderMode (NTSC 640x480).
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

void HuFaultInitXfbDirectDraw(GXRenderModeObj *mode) { (void)mode; }
void HuFaultSetXfbAddress(int idx, void *addr) { (void)idx; (void)addr; }
void HuDvdErrDispInit(GXRenderModeObj *mode, void *fb1, void *fb2) { (void)mode; (void)fb1; (void)fb2; }
void HuMemInitAll(void) {}
void HuAudInit(void) {}
void HuARInit(void) {}
void HuCardInit(void) {}
void DEMOUpdateStats(int which) { (void)which; }
void DEMOPrintStats(void) {}
uint32_t frand(void) { return 0; }
void msmSysRegularProc(void) {}
int HuDvdErrWait = 0;

// GWInit dependencies (game-specific, not Nintendo SDK).
void _InitFlag(void) {}
int _CheckFlag(int id) { (void)id; return 0; }
void GWGameStatReset(void) {}
void GWRumbleSet(int enabled) { (void)enabled; }
void GWMGExplainSet(int enabled) { (void)enabled; }
void GWMGShowComSet(int enabled) { (void)enabled; }
void GWMessSpeedSet(int speed) { (void)speed; }
void GWSaveModeSet(int mode) { (void)mode; }
void GWLanguageSet(int16_t language) { (void)language; }

// ---- host scenario glue (tests/harness/gc_host_runner.c) ----

const char *gc_scenario_label(void) { return "workload/mp4_init_to_viwait_001"; }
const char *gc_scenario_out_path(void) { return "../../actual/workload/mp4_init_to_viwait_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    // Mirror MP4 main() init order up to the first VIWaitForRetrace,
    // but stub heavy game modules after HuSprInit.
    HuSysInit(&GXNtsc480IntDf);
    HuPrcInit();
    HuPadInit();
    GWInit();
    pfInit();
    HuSprInit();

    Hu3DInit();
    HuDataInit();
    HuPerfInit();
    (void)HuPerfCreate("USR0", 0xFF, 0xFF, 0xFF, 0xFF);
    (void)HuPerfCreate("USR1", 0x00, 0xFF, 0xFF, 0xFF);
    WipeInit(0);
    omMasterInit(0, 0, 0, 0);

    // First hard SDK "checkpoint" after init chain.
    VIWaitForRetrace();

    // Marker: prove chain returned.
    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x10);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0x00, 0x4D503436u); // "MP46"
    wr32be(p + 0x04, 0xDEADBEEFu);
}

