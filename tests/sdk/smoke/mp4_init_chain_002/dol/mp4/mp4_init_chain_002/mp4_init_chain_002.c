#include <stdint.h>

// This smoke test runs our sdk_port implementation on real PPC (in Dolphin).
// The oracle for phase A is "PPC build of sdk_port" vs "host build of sdk_port".
//
// This does NOT prove correctness vs the retail MP4 DOL. Phase B is for that.

#include "src/sdk_port/gc_mem.c"

#include "sdk_state.h"

#include "src/sdk_port/os/OSArena.c"
#include "src/sdk_port/os/OSAlloc.c"
#include "src/sdk_port/os/OSInit.c"
#include "src/sdk_port/os/OSFastCast.c"
#include "src/sdk_port/os/OSError.c"
#include "src/sdk_port/os/OSSystem.c"
#include "src/sdk_port/os/OSRtc.c"
#include "src/sdk_port/os/OSCache.c"

#include "src/sdk_port/dvd/DVD.c"
#include "src/sdk_port/vi/VI.c"
#include "src/sdk_port/pad/PAD.c"
#include "src/sdk_port/gx/GX.c"

// Snapshot region inside MEM1 that we *do* compare bit-exact between
// Dolphin (PPC) and host. Full MEM1 dumps will also include lots of unrelated
// bytes (code, loader state, etc) that are not meaningful to compare yet.
enum { SNAPSHOT_ADDR = 0x80300100u };

static inline void gc_disable_interrupts(void) {
  // Prevent periodic interrupts (e.g. decrementer) from jumping to uninitialized
  // exception vectors. These smoke DOLs are not a full OS environment.
  uint32_t msr;
  __asm__ volatile("mfmsr %0" : "=r"(msr));
  msr &= ~0x8000u; // MSR[EE]
  __asm__ volatile("mtmsr %0; isync" : : "r"(msr));
}

static inline void gc_arm_decrementer_far_future(void) {
  // Also push the decrementer far away in case EE gets enabled by called code.
  __asm__ volatile(
      "lis 3,0x7fff\n"
      "ori 3,3,0xffff\n"
      "mtdec 3\n"
      :
      :
      : "r3");
}

static inline void gc_safepoint(void) {
  // Called code may enable interrupts and/or program the decrementer.
  // Force a stable, non-interrupting environment for our smoke harness.
  gc_disable_interrupts();
  gc_arm_decrementer_far_future();
}

static inline void store_u32be_ptr(volatile uint8_t *p, uint32_t v) {
  p[0] = (uint8_t)(v >> 24);
  p[1] = (uint8_t)(v >> 16);
  p[2] = (uint8_t)(v >> 8);
  p[3] = (uint8_t)(v >> 0);
}

static void emit_snapshot(void) {
  volatile uint8_t *out = (volatile uint8_t *)SNAPSHOT_ADDR;

  // Header
  store_u32be_ptr(out + 0x00, 0x534D4F4Bu); // "SMOK"
  store_u32be_ptr(out + 0x04, 0x00000001u); // version
  store_u32be_ptr(out + 0x08, (uint32_t)(uintptr_t)OSGetArenaLo());
  store_u32be_ptr(out + 0x0C, (uint32_t)(uintptr_t)OSGetArenaHi());

  // A few key side-effect counters/state used by early init.
  extern uint32_t gc_dvd_initialized;
  extern uint32_t gc_dvd_drive_status;
  store_u32be_ptr(out + 0x10, gc_dvd_initialized);
  store_u32be_ptr(out + 0x14, gc_dvd_drive_status);

  extern uint32_t gc_vi_change_mode;
  extern uint32_t gc_vi_disp_pos_x;
  extern uint32_t gc_vi_disp_pos_y;
  extern uint32_t gc_vi_disp_size_x;
  extern uint32_t gc_vi_disp_size_y;
  extern uint32_t gc_vi_fb_size_x;
  extern uint32_t gc_vi_fb_size_y;
  extern uint32_t gc_vi_xfb_mode;
  extern uint32_t gc_vi_helper_calls;
  store_u32be_ptr(out + 0x18, gc_vi_change_mode);
  store_u32be_ptr(out + 0x1C, gc_vi_disp_pos_x);
  store_u32be_ptr(out + 0x20, gc_vi_disp_pos_y);
  store_u32be_ptr(out + 0x24, gc_vi_disp_size_x);
  store_u32be_ptr(out + 0x28, gc_vi_disp_size_y);
  store_u32be_ptr(out + 0x2C, gc_vi_fb_size_x);
  store_u32be_ptr(out + 0x30, gc_vi_fb_size_y);
  store_u32be_ptr(out + 0x34, gc_vi_xfb_mode);
  store_u32be_ptr(out + 0x38, gc_vi_helper_calls);

  extern uint32_t gc_pad_initialized;
  store_u32be_ptr(out + 0x3C, gc_pad_initialized);

  extern uint32_t gc_gx_cp_disp_src;
  extern uint32_t gc_gx_cp_disp_size;
  extern uint32_t gc_gx_copy_disp_dest;
  store_u32be_ptr(out + 0x40, gc_gx_cp_disp_src);
  store_u32be_ptr(out + 0x44, gc_gx_cp_disp_size);
  store_u32be_ptr(out + 0x48, gc_gx_copy_disp_dest);
}

int main(void) {
  // Make this DOL robust against periodic exceptions while we are dumping MEM1.
  //
  // NOTE: Do NOT write low exception vectors (e.g. 0x00000900) because Dolphin
  // can warn about invalid low-memory writes unless MMU is enabled. Instead, we
  // prevent the interrupt from happening in the first place by forcing a
  // safepoint before/after each SDK call.
  gc_safepoint();

  // Wire sdk_port virtual RAM to MEM1.
  gc_mem_set(0x80000000u, 0x01800000u, (uint8_t *)0x80000000u);
  gc_sdk_state_reset();

  // MP4 init chain smoke (approx HuSysInit + early DEMO/InitMem/InitVI).
  //
  // The goal is to validate *combined* SDK side effects, not just single-call microtests.
  // Arguments are chosen to mirror MP4's decomp where practical (see decomp_mario_party_4/src/game/init.c).
  OSInit();
  gc_safepoint();
  DVDInit();
  gc_safepoint();
  VIInit();
  gc_safepoint();
  PADInit();
  gc_safepoint();

  (void)OSGetProgressiveMode();
  gc_safepoint();
  (void)VIGetDTVStatus();
  gc_safepoint();

  // Provide deterministic GXRenderModeObj fields so VIConfigure/GXAdjustForOverscan
  // produce deterministic side effects.
  typedef struct {
    uint32_t viTVmode;
    uint16_t fbWidth;
    uint16_t efbHeight;
    uint16_t xfbHeight;
    uint16_t viXOrigin;
    uint16_t viYOrigin;
    uint16_t viWidth;
    uint16_t viHeight;
    uint32_t xFBmode;
  } GXRenderModeObjLocal;

  static uint8_t dummy_rmode[256] __attribute__((aligned(32)));
  GXRenderModeObjLocal *rm = (GXRenderModeObjLocal *)dummy_rmode;
  // Minimal zero-init without libc.
  for (uint32_t i = 0; i < (uint32_t)sizeof(*rm); i++) ((volatile uint8_t *)rm)[i] = 0;
  rm->viTVmode = 0;
  rm->fbWidth = 640;
  rm->efbHeight = 480;
  rm->xfbHeight = 480;
  rm->viXOrigin = 0;
  rm->viYOrigin = 0;
  rm->viWidth = 640;
  rm->viHeight = 480;
  rm->xFBmode = 1;

  VIConfigure(dummy_rmode);
  gc_safepoint();
  VIConfigurePan(0, 0, 0, 0);
  gc_safepoint();

  (void)OSAlloc(0x100);
  gc_safepoint();

  GXInit((void *)0x81000000u, 0x10000);
  gc_safepoint();

  OSInitFastCast();
  gc_safepoint();
  (void)VIGetTvFormat();
  gc_safepoint();

  uint8_t overscan_out[256] __attribute__((aligned(32)));
  GXAdjustForOverscan((GXRenderModeObj *)dummy_rmode, (GXRenderModeObj *)overscan_out, 0, 0);
  gc_safepoint();
  GXSetViewport(0.0f, 0.0f, 640.0f, 480.0f, 0.0f, 1.0f);
  gc_safepoint();
  GXSetScissor(0, 0, 640, 480);
  gc_safepoint();
  GXSetDispCopySrc(0, 0, 640, 480);
  gc_safepoint();
  GXSetDispCopyDst(640, 480);
  gc_safepoint();
  (void)GXSetDispCopyYScale(1.0f);
  gc_safepoint();

  // ---- InitGX tail (see decomp init.c: GXGetYScaleFactor / GXSetCopyFilter / GXSetPixelFmt / GXCopyDisp / Gamma) ----
  (void)GXGetYScaleFactor(480, 480);
  gc_safepoint();

  static const uint8_t sp[12][2] = {
      {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6},
      {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6},
  };
  static const uint8_t vf[7] = {0, 0, 21, 22, 21, 0, 0};
  GXSetCopyFilter(0, sp, 1, vf);
  gc_safepoint();

  // MP4 uses AA conditional pixel format; our dummy rmode has aa=0 so take RGB8_Z24 path.
  GXSetPixelFmt(2 /*GX_PF_RGB8_Z24*/, 0 /*GX_ZC_LINEAR*/);
  gc_safepoint();

  // ---- InitMem (see decomp init.c) ----
  void *arena_lo = OSGetArenaLo();
  void *arena_hi = OSGetArenaHi();
  gc_safepoint();

  // fb_size = round16(fbWidth) * xfbHeight * 2
  uint32_t fb_width = 640;
  uint32_t xfb_height = 480;
  uint32_t fb_size = ((fb_width + 15u) & ~15u) * xfb_height * 2u;

  void *fb1 = (void *)(uintptr_t)OSRoundUp32B((uint32_t)(uintptr_t)arena_lo);
  gc_safepoint();
  void *fb2 = (void *)(uintptr_t)OSRoundUp32B((uint32_t)(uintptr_t)fb1 + fb_size);
  gc_safepoint();
  void *demo_current = fb2;

  // Mirror InitGX: copy to current buffer and set gamma (MP4 uses DemoCurrentBuffer).
  GXCopyDisp(demo_current, 1);
  gc_safepoint();
  GXSetDispCopyGamma(0 /*GX_GM_1_0*/);
  gc_safepoint();

  // Exercise the same writeback call MP4 uses on PAL; harmless no-op in our port but observable.
  DCStoreRangeNoSync(fb1, fb_size);
  gc_safepoint();

  // arena_lo = roundUp(fb2 + fb_size), OSSetArenaLo(arena_lo)
  arena_lo = (void *)(uintptr_t)OSRoundUp32B((uint32_t)(uintptr_t)fb2 + fb_size);
  gc_safepoint();
  OSSetArenaLo(arena_lo);
  gc_safepoint();

  // Query console/memory info (drives InitMem condition in MP4).
  (void)OSGetConsoleType();
  gc_safepoint();
  (void)OSGetPhysicalMemSize();
  gc_safepoint();
  (void)OSGetConsoleSimulatedMemSize();
  gc_safepoint();

  // OS init-heap setup: arena_lo/hi, OSInitAlloc, OSSetArenaLo, round up/down, OSCreateHeap + OSSetCurrentHeap
  arena_lo = OSGetArenaLo();
  arena_hi = OSGetArenaHi();
  gc_safepoint();
  arena_lo = OSInitAlloc(arena_lo, arena_hi, 1);
  gc_safepoint();
  OSSetArenaLo(arena_lo);
  gc_safepoint();
  arena_lo = (void *)(uintptr_t)OSRoundUp32B((uint32_t)(uintptr_t)arena_lo);
  gc_safepoint();
  arena_hi = (void *)(uintptr_t)OSRoundDown32B((uint32_t)(uintptr_t)arena_hi);
  gc_safepoint();
  OSSetCurrentHeap(OSCreateHeap(arena_lo, arena_hi));
  gc_safepoint();

  // End of InitMem: arena_lo = arena_hi; OSSetArenaLo(arena_lo)
  OSSetArenaLo(arena_hi);
  gc_safepoint();

  // ---- InitVI (see decomp init.c) ----
  VISetNextFrameBuffer(fb1);
  gc_safepoint();
  demo_current = fb2;
  (void)demo_current;
  VIFlush();
  gc_safepoint();
  VIWaitForRetrace();
  gc_safepoint();

  // Marker in MEM1 for sanity.
  *(volatile uint32_t *)0x80300000 = 0xC0DEF00Du;
  *(volatile uint32_t *)0x80300004 = 0x00000028u; // 40-ish calls (kept as a rough progress counter)

  emit_snapshot();

  // Some SDK code paths can temporarily enable interrupts; keep the harness in a
  // stable, non-interrupting state while Dolphin/GDB is dumping memory.
  gc_safepoint();

  while (1) {
    gc_safepoint();
    __asm__ volatile("nop");
  }
  return 0;
}
