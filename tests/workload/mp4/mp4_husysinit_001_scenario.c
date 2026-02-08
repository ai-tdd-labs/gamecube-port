#include <stdint.h>

// Host harness helpers.
#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

// This is an integration workload smoke harness, not a correctness oracle.
// It compiles (a subset of) MP4 decomp code on host and runs HuSysInit to
// validate reachability and discover missing SDK surface area.
//
// Ground truth for SDK behavior remains: DOL-in-Dolphin expected.bin vs
// host sdk_port actual.bin unit tests.

// Decomp headers (via tools/run_host_scenario.sh workload include paths)
#include "dolphin/gx.h"

// MP4 decomp function (compiled in via extra_srcs in tools/run_host_scenario.sh)
void HuSysInit(GXRenderModeObj *mode);

// ---- Game-specific externs referenced by MP4 init.c ----
// Stubbed to keep the harness focused on SDK reachability.

// Minimal render modes required by HuSysInit/InitRenderMode.
// Values are chosen to keep InitMem calculations sane (NTSC 640x480).
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

// MP4 uses frand() only to seed runtime state; any deterministic value is fine here.
uint32_t frand(void) { return 0; }

// ---- host scenario glue (tests/harness/gc_host_runner.c) ----

const char *gc_scenario_label(void) { return "workload/mp4_husysinit_001"; }
const char *gc_scenario_out_path(void) { return "../../actual/workload/mp4_husysinit_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    // MP4 main() passes &GXNtsc480IntDf (NTSC build). In decomp headers this is global.
    // If not present for some reason, passing NULL will cause HuSysInit to select a mode
    // via VIGetTvFormat() (still fine for reachability).
    extern GXRenderModeObj GXNtsc480IntDf;
    HuSysInit(&GXNtsc480IntDf);

    // Marker: prove HuSysInit returned.
    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x10);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0x00, 0x4D503430u); // "MP40"
    wr32be(p + 0x04, 0xDEADBEEFu);
}
