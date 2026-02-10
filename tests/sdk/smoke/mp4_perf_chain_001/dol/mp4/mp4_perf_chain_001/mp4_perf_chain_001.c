#include <stdint.h>

// This smoke test runs our sdk_port implementation on real PPC (in Dolphin).
// The oracle for phase A is "PPC build of sdk_port" vs "host build of sdk_port".
//
// This does NOT prove correctness vs the retail MP4 DOL. Phase B is for that.
//
// Important: compile sdk_port modules as separate translation units (oracle_*.c)
// to avoid macro/enum clashes when including decompiled-style sources.

#include "sdk_state.h"

void gc_mem_set(uint32_t base, uint32_t size, uint8_t *host_ptr);

void OSInit(void);
void OSSetArenaLo(void *addr);
void OSSetArenaHi(void *addr);
void OSReport(const char *fmt, ...);

// Stopwatch (MP4 perf bookkeeping)
typedef struct OSStopwatch {
  char *name;
  uint64_t total;
  uint32_t hits;
  uint32_t running;
  uint64_t last;
  uint64_t min;
  uint64_t max;
} OSStopwatch;

void OSInitStopwatch(OSStopwatch *sw, char *name);
void OSStartStopwatch(OSStopwatch *sw);
void OSStopStopwatch(OSStopwatch *sw);
long long OSCheckStopwatch(OSStopwatch *sw);
void OSResetStopwatch(OSStopwatch *sw);

// GX draw sync (MP4 perf)
typedef void (*GXDrawSyncCallback)(uint16_t token);
GXDrawSyncCallback GXSetDrawSyncCallback(GXDrawSyncCallback cb);
void GXSetDrawSync(uint16_t token);

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

static void DrawSyncCB(uint16_t token) { (void)token; }

static void emit_snapshot(const OSStopwatch *sw) {
  volatile uint8_t *out = (volatile uint8_t *)SNAPSHOT_ADDR;

  // Header
  store_u32be_ptr(out + 0x00, 0x534D4F4Bu); // "SMOK"
  store_u32be_ptr(out + 0x04, 0x00000001u); // version

  // Stopwatch results (deterministic OSGetTime counter).
  store_u32be_ptr(out + 0x08, (uint32_t)(sw->total & 0xFFFFFFFFu));
  store_u32be_ptr(out + 0x0C, sw->hits);
  store_u32be_ptr(out + 0x10, sw->running);
  store_u32be_ptr(out + 0x14, (uint32_t)(sw->min & 0xFFFFFFFFu));
  store_u32be_ptr(out + 0x18, (uint32_t)(sw->max & 0xFFFFFFFFu));

  // GX observable state.
  extern uint32_t gc_gx_last_draw_sync_token;
  extern uint32_t gc_gx_last_ras_reg;
  store_u32be_ptr(out + 0x1C, gc_gx_last_draw_sync_token);
  store_u32be_ptr(out + 0x20, gc_gx_last_ras_reg);

  // Reserve the remainder (zero).
  for (uint32_t off = 0x24; off < 0x40; off += 4) store_u32be_ptr(out + off, 0);
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

  // MP4 perf chain primitives (HuPerfInit/HuPerfBegin style):
  // - OSStopwatch bookkeeping
  // - GX draw sync callback + token
  OSInit();
  gc_safepoint();

  // Pin arenas for determinism (even though we do not snapshot them here).
  OSSetArenaLo((void *)0x80026000u);
  OSSetArenaHi((void *)0x81700000u);
  gc_safepoint();

  OSStopwatch sw;
  OSInitStopwatch(&sw, 0);
  gc_safepoint();
  OSResetStopwatch(&sw);
  gc_safepoint();
  OSStartStopwatch(&sw);
  gc_safepoint();

  (void)GXSetDrawSyncCallback(DrawSyncCB);
  gc_safepoint();
  (void)GXSetDrawSyncCallback(0);
  gc_safepoint();
  GXSetDrawSync(0x1234);
  gc_safepoint();

  OSStopStopwatch(&sw);
  gc_safepoint();
  (void)OSCheckStopwatch(&sw);
  gc_safepoint();

  OSReport("mp4_perf_chain_001 done\n");
  gc_safepoint();

  // Marker in MEM1 for sanity.
  *(volatile uint32_t *)0x80300000 = 0xC0DEF00Du;
  *(volatile uint32_t *)0x80300004 = 0x00000020u; // suite id / step count (must match host)

  emit_snapshot(&sw);

  while (1) {
    // Keep the decrementer far in the future and EE disabled. Some called code
    // may re-arm DEC or re-enable interrupts, and Dolphin will trap to 0x900 if
    // the decrementer exception fires before any handlers are installed.
    gc_safepoint();
  }
  return 0;
}
