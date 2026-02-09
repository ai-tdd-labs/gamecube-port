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
void Hu3DInit(void);

// ---- Game-specific externs referenced by MP4 init/pad/process/gwinit ----
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

void _InitFlag(void) {}
int _CheckFlag(int id) { (void)id; return 0; }
void GWGameStatReset(void) {}
void GWRumbleSet(int enabled) { (void)enabled; }
void GWMGExplainSet(int enabled) { (void)enabled; }
void GWMGShowComSet(int enabled) { (void)enabled; }
void GWMessSpeedSet(int speed) { (void)speed; }
void GWSaveModeSet(int mode) { (void)mode; }
void GWLanguageSet(int16_t language) { (void)language; }

const char *gc_scenario_label(void) { return "workload/mp4_hu3dinit_001"; }
const char *gc_scenario_out_path(void) { return "../../actual/workload/mp4_hu3dinit_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    HuSysInit(&GXNtsc480IntDf);
    HuPrcInit();
    HuPadInit();
    GWInit();
    pfInit();
    HuSprInit();

    // Checkpoint: state *after* HuSprInit, *at* Hu3DInit completion.
    Hu3DInit();

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x10);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0x00, 0x4D503443u); // "MP4C" (checkpoint)
    wr32be(p + 0x04, 0xDEADBEEFu);
}

