#include <stdint.h>

// Expected oracle for sdk_port DVD state model.
#include "src/sdk_port/gc_mem.c"
#include "src/sdk_port/sdk_state.h"
#include "src/sdk_port/dvd/DVD.c"

static inline void store_u32be_ptr(volatile uint8_t *p, uint32_t v) {
  p[0] = (uint8_t)(v >> 24);
  p[1] = (uint8_t)(v >> 16);
  p[2] = (uint8_t)(v >> 8);
  p[3] = (uint8_t)(v >> 0);
}

static inline void gc_disable_interrupts(void) {
  uint32_t msr;
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

int main(void) {
  // Stable environment for tiny DOLs.
  gc_safepoint();

  // Wire sdk_port virtual RAM to MEM1.
  gc_mem_set(0x80000000u, 0x01800000u, (uint8_t *)0x80000000u);
  gc_sdk_state_reset();

  // MP4 normal boot path: no executing command block when HuDvdErrorWatch polls.
  DVDInit();
  uint32_t st = DVDGetDriveStatus();

  volatile uint8_t *out = (volatile uint8_t *)0x80300000u;
  store_u32be_ptr(out + 0x00, 0xDEADBEEFu);
  store_u32be_ptr(out + 0x04, st);

  while (1) {
    gc_safepoint();
    __asm__ volatile("nop");
  }
  return 0;
}
