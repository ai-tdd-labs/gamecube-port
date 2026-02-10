#include <stdint.h>

// This smoke chain runs our sdk_port implementation on real PPC (in Dolphin).
// Oracle for phase A: "PPC build of sdk_port" vs "host build of sdk_port".
//
// It models the MP4 HuData* DVD read subset:
//   DVDFastOpen -> OSRoundUp32B -> DCInvalidateRange -> DVDReadAsync
//   -> DVDGetCommandBlockStatus -> DVDClose -> OSRoundDown32B

#include "src/sdk_port/sdk_state.h"

typedef uint32_t u32;
typedef int32_t s32;

typedef struct { volatile s32 state; } DVDCommandBlock;
typedef struct {
  DVDCommandBlock cb;
  u32 startAddr;
  u32 length;
  s32 entrynum;
} DVDFileInfo;

typedef void (*DVDCallback)(s32 result, DVDFileInfo* fileInfo);

// sdk_port test hooks
void gc_dvd_test_reset_files(void);
void gc_dvd_test_set_file(s32 entrynum, const void *data, u32 len);

// OS / cache / DVD
u32 OSRoundUp32B(u32 x);
u32 OSRoundDown32B(u32 x);

int DVDFastOpen(s32 entrynum, DVDFileInfo *file);
s32 DVDReadAsync(DVDFileInfo *file, void *addr, s32 len, s32 offset, DVDCallback cb);
int DVDGetCommandBlockStatus(DVDCommandBlock *block);
int DVDClose(DVDFileInfo *file);

// We cannot override libogc's DCInvalidateRange without link conflicts.
// For this smoke chain we record the intended invalidate parameters in the
// snapshot window.
static u32 g_dc_inval_last_addr;
static u32 g_dc_inval_last_len;

static void gc_cache_record_invalidate(void *addr, u32 nbytes) {
  g_dc_inval_last_addr = (u32)(uintptr_t)addr;
  g_dc_inval_last_len = nbytes;
}

enum { SNAPSHOT_ADDR = 0x80300100u };

static inline void gc_disable_interrupts(void) {
  u32 msr;
  __asm__ volatile("mfmsr %0" : "=r"(msr));
  msr &= ~0x8000u; // MSR[EE]
  __asm__ volatile("mtmsr %0; isync" : : "r"(msr));
}

static inline void gc_arm_decrementer_far_future(void) {
  __asm__ volatile(
      "lis 3,0x7fff\n"
      "ori 3,3,0xffff\n"
      "mtdec 3\n"
      :
      :
      : "r3");
}

static inline void gc_safepoint(void) {
  gc_disable_interrupts();
  gc_arm_decrementer_far_future();
}

static inline void store_u32be_ptr(volatile uint8_t *p, u32 v) {
  p[0] = (uint8_t)(v >> 24);
  p[1] = (uint8_t)(v >> 16);
  p[2] = (uint8_t)(v >> 8);
  p[3] = (uint8_t)(v >> 0);
}

static void emit_snapshot(void) {
  volatile uint8_t *out = (volatile uint8_t *)SNAPSHOT_ADDR;

  store_u32be_ptr(out + 0x00, 0x534D4F4Bu); // "SMOK"
  store_u32be_ptr(out + 0x04, 0x00000001u); // version

  store_u32be_ptr(out + 0x08, g_dc_inval_last_addr);
  store_u32be_ptr(out + 0x0C, g_dc_inval_last_len);

  store_u32be_ptr(out + 0x10, gc_sdk_state_load_u32be(GC_SDK_OFF_DVD_READ_CALLS));
  store_u32be_ptr(out + 0x14, gc_sdk_state_load_u32be(GC_SDK_OFF_DVD_CLOSE_CALLS));
  store_u32be_ptr(out + 0x18, gc_sdk_state_load_u32be(GC_SDK_OFF_DVD_LAST_READ_LEN));
  store_u32be_ptr(out + 0x1C, gc_sdk_state_load_u32be(GC_SDK_OFF_DVD_LAST_READ_OFF));
}

int main(void) {
  gc_safepoint();

  gc_mem_set(0x80000000u, 0x01800000u, (uint8_t *)0x80000000u);
  gc_sdk_state_reset();

  // deterministic file contents (0..255)
  static uint8_t k_file[0x100];
  for (u32 i = 0; i < (u32)sizeof(k_file); i++) k_file[i] = (uint8_t)i;

  gc_dvd_test_reset_files();
  gc_dvd_test_set_file(0, k_file, (u32)sizeof(k_file));

  DVDFileInfo fi;
  int ok = DVDFastOpen(0, &fi);

  volatile uint8_t *dest = (volatile uint8_t *)0x80400000u;
  for (u32 i = 0; i < 0x100u; i++) dest[i] = 0xAA;

  u32 len = 0x23u;
  u32 aligned_up = OSRoundUp32B(len);
  (void)OSRoundDown32B(0x817FFFFFu);

  gc_cache_record_invalidate((void *)dest, aligned_up);
  s32 r = DVDReadAsync(&fi, (void *)dest, (s32)aligned_up, 0, (DVDCallback)0);
  int st = DVDGetCommandBlockStatus(&fi.cb);
  (void)DVDClose(&fi);

  *(volatile u32 *)0x80300000 = 0xC0DEF00Du;
  *(volatile u32 *)0x80300004 = (u32)ok ^ ((u32)r << 8) ^ ((u32)st << 16);
  emit_snapshot();

  while (1) {
    gc_safepoint();
    __asm__ volatile("nop");
  }
  return 0;
}
