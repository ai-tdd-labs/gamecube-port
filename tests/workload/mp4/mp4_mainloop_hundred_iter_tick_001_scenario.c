#include <stdint.h>

// Host harness helpers.
#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

// RAM-backed SDK state helpers.
#include "sdk_state.h"

// Decomp headers (via tools/run_host_scenario.sh workload include paths)
#include "dolphin/gx.h"

// MP4 decomp/workload functions (compiled in via extra_srcs in tools/run_host_scenario.sh)
void HuSysInit(GXRenderModeObj *mode);
void HuPrcInit(void);
void HuPrcCall(int tick);
void HuPadInit(void);
void GWInit(void);
void pfInit(void);
void HuSprInit(void);
void HuPadRead(void);
void pfClsScr(void);

// Minimal MP4 slice (tests/workload/mp4/slices/husoftresetbuttoncheck_only.c)
int HuSoftResetButtonCheck(void);

// Pad globals (game-specific) updated by PadReadVSync -> HuPadRead.
extern int8_t HuPadErr[4];

// Host-only stubs (tests/workload/mp4/slices/post_sprinit_stubs.c)
void Hu3DInit(void);
void HuDataInit(void);
void HuPerfInit(void);
int HuPerfCreate(const char *name, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void WipeInit(GXRenderModeObj *rmode);
void omMasterInit(int a0, void *ovltbl, int ovl_count, int ovl_boot);

// Nintendo SDK (sdk_port)
void VIWaitForRetrace(void);
int VIGetNextField(void);
int VIGetRetraceCount(void);
void OSReport(const char *fmt, ...);

// GX metric calls used in MP4 main loop.
void GXSetGPMetric(int perf0, int perf1);
void GXClearGPMetric(void);
void GXSetVCacheMetric(int attr);
void GXClearVCacheMetric(void);
void GXClearPixMetric(void);
void GXClearMemMetric(void);
void GXReadGPMetric(uint32_t *a, uint32_t *b);
void GXReadVCacheMetric(uint32_t *check, uint32_t *miss, uint32_t *stall);
void GXReadPixMetric(uint32_t *top_in, uint32_t *top_out, uint32_t *bot_in, uint32_t *bot_out,
                     uint32_t *clr_in, uint32_t *copy_clks);
void GXReadMemMetric(uint32_t *cp_req, uint32_t *tc_req, uint32_t *cpu_rd_req, uint32_t *cpu_wr_req,
                     uint32_t *dsp_req, uint32_t *io_req, uint32_t *vi_req, uint32_t *pe_req,
                     uint32_t *rf_req, uint32_t *fi_req);

// ---- Minimal game stubs for one-loop execution ----

static void HuPerfZero(void) {}
static void HuPerfBegin(int which) { (void)which; }
static void HuPerfEnd(int which) { (void)which; }
static void MGSeqMain(void) {}
static void Hu3DExec(void) {}
void WipeExecAlways(void);
void pfDrawFonts(void);
void msmMusFdoutEnd(void);

// Real MP4 functions from vendor init.c (already compiled in workload via tools/run_host_scenario.sh).
void HuSysBeforeRender(void);
void HuSysDoneRender(int retrace_count);

// Minimal MP4 slice (tests/workload/mp4/slices/hu3d_preproc_only.c)
void Hu3DPreProc(void);

// Minimal MP4 slice (tests/workload/mp4/slices/hudvderrorwatch_only.c)
void HuDvdErrorWatch(void);

// This global is referenced by older chain code; keep it consistent.
int HuDvdErrWait = 0;

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

const char *gc_scenario_label(void) { return "workload/mp4_mainloop_hundred_iter_tick_001"; }
const char *gc_scenario_out_path(void) { return "../../actual/workload/mp4_mainloop_hundred_iter_tick_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    // Init chain (MP4 order), then stub the heavy modules and execute 100 loop bodies.
    (void)ram;

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
    WipeInit(&GXNtsc480IntDf);
    omMasterInit(0, 0, 0, 0);

    // Align with MP4 main() init: first retrace and field check.
    VIWaitForRetrace();
    if (VIGetNextField() == 0) {
        OSReport("VI_FIELD_BELOW\\n");
        VIWaitForRetrace();
    }

    for (int iter = 0; iter < 100; iter++) {
        // Simulate a VI retrace tick so the PostRetraceCallback installed by
        // HuPadInit (PadReadVSync) runs each frame.
        VIWaitForRetrace();

        int retrace = VIGetRetraceCount();
        if (HuSoftResetButtonCheck() != 0 || HuDvdErrWait != 0) {
            // Shouldn't happen with our stubs, but keep behavior explicit.
        } else {
            uint32_t met0 = 0, met1 = 0;
            uint32_t vcheck = 0, vmiss = 0, vstall = 0;
            uint32_t top_in = 0, top_out = 0, bot_in = 0, bot_out = 0, clr_in = 0, copy_clks = 0;
            uint32_t cp_req = 0, tc_req = 0, cpu_rd_req = 0, cpu_wr_req = 0, dsp_req = 0, io_req = 0;
            uint32_t vi_req = 0, pe_req = 0, rf_req = 0, fi_req = 0;

            HuPerfZero();
            HuPerfBegin(2);
            HuSysBeforeRender();
            GXSetGPMetric(0, 0);
            GXClearGPMetric();
            GXSetVCacheMetric(0);
            GXClearVCacheMetric();
            GXClearPixMetric();
            GXClearMemMetric();
            HuPerfBegin(0);
            Hu3DPreProc();
            HuPadRead();
            pfClsScr();
            HuPrcCall(1);
            MGSeqMain();
            HuPerfBegin(1);
            Hu3DExec();
            HuDvdErrorWatch();
            WipeExecAlways();
            HuPerfEnd(0);
            pfDrawFonts();
            HuPerfEnd(1);
            msmMusFdoutEnd();
            HuSysDoneRender(retrace);
            GXReadGPMetric(&met0, &met1);
            GXReadVCacheMetric(&vcheck, &vmiss, &vstall);
            GXReadPixMetric(&top_in, &top_out, &bot_in, &bot_out, &clr_in, &copy_clks);
            GXReadMemMetric(&cp_req, &tc_req, &cpu_rd_req, &cpu_wr_req, &dsp_req, &io_req, &vi_req, &pe_req, &rf_req, &fi_req);
            HuPerfEnd(2);
        }
    }

    // Marker: prove we got through 100 loop bodies.
    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x10);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0x00, 0x4D503459u); // "MP4Y" (pad tick x100)
    wr32be(p + 0x04, 0xDEADBEEFu);
    wr32be(p + 0x08, (uint32_t)(uint8_t)HuPadErr[0]);
    wr32be(p + 0x0C, gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_READ_CALLS));
}

