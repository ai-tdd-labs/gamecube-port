#include <stdint.h>

// Host harness helpers.
#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

// This is an integration workload smoke harness, not a correctness oracle.
// It compiles MP4 decomp code on host and runs the early init path:
//   HuSysInit() -> HuPadInit()
//
// Ground truth for SDK behavior remains: DOL-in-Dolphin expected.bin vs
// host sdk_port actual.bin unit tests.

// Decomp headers (via tools/run_host_scenario.sh workload include paths)
#include "dolphin/gx.h"
#include "game/pad.h"

// MP4 decomp functions (compiled in via extra_srcs in tools/run_host_scenario.sh)
void HuSysInit(GXRenderModeObj *mode);

// ---- Game-specific externs referenced by MP4 init.c / pad.c ----

// Minimal render modes required by HuSysInit/InitRenderMode.
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

// pad.c references this as a runtime gate for reading inputs during DVD error screens.
int HuDvdErrWait = 0;

// MSM stub: pad.c calls msmSysRegularProc() during read/update.
void msmSysRegularProc(void) {}

// ---- host scenario glue (tests/harness/gc_host_runner.c) ----

const char *gc_scenario_label(void) { return "workload/mp4_hupadinit_001"; }
const char *gc_scenario_out_path(void) { return "../../actual/workload/mp4_hupadinit_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    // MP4 main() passes &GXNtsc480IntDf (NTSC build).
    HuSysInit(&GXNtsc480IntDf);

    // Next MP4 init step: pad init.
    HuPadInit();

    // Marker: prove HuPadInit returned.
    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x10);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0x00, 0x4D503431u); // "MP41"
    wr32be(p + 0x04, 0xDEADBEEFu);
}
