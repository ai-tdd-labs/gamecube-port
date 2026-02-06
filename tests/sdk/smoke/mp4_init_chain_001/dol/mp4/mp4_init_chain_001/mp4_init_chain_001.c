#include <stdint.h>

// This smoke test runs our sdk_port implementation on real PPC (in Dolphin).
// The oracle for phase A is "PPC build of sdk_port" vs "host build of sdk_port".
//
// This does NOT prove correctness vs the retail MP4 DOL. Phase B is for that.

#include "src/sdk_port/gc_mem.c"

#include "src/sdk_port/os/OSArena.c"
#include "src/sdk_port/os/OSAlloc.c"
#include "src/sdk_port/os/OSInit.c"
#include "src/sdk_port/os/OSFastCast.c"
#include "src/sdk_port/os/OSError.c"
#include "src/sdk_port/os/OSSystem.c"
#include "src/sdk_port/os/OSRtc.c"

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

  // First 20 MP4 HuSysInit SDK calls (from docs/sdk/mp4/MP4_chain_all.csv).
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

  OSReport("mp4_init_chain_001 done\n");
  gc_safepoint();

  // Marker in MEM1 for sanity.
  *(volatile uint32_t *)0x80300000 = 0xC0DEF00Du;
  *(volatile uint32_t *)0x80300004 = 0x00000014u; // 20 calls

  emit_snapshot();

  while (1) {
    __asm__ volatile("nop");
  }
  return 0;
}
