#include <stdint.h>

// Expected oracle for sdk_port interrupt state model.
#include "src/sdk_port/gc_mem.c"
#include "src/sdk_port/sdk_state.h"
#include "src/sdk_port/os/OSInterrupts.c"

static inline void store_u32be_ptr(volatile uint8_t *p, uint32_t v) {
  p[0] = (uint8_t)(v >> 24);
  p[1] = (uint8_t)(v >> 16);
  p[2] = (uint8_t)(v >> 8);
  p[3] = (uint8_t)(v >> 0);
}

int main(void) {
  gc_mem_set(0x80000000u, 0x01800000u, (uint8_t *)0x80000000u);
  gc_sdk_state_reset();

  int outer = OSDisableInterrupts(); // expect 1
  int inner = OSDisableInterrupts(); // expect 0
  int prev1 = OSRestoreInterrupts(inner);
  int prev2 = OSRestoreInterrupts(outer);

  uint32_t enabled = gc_sdk_state_load_u32be(GC_SDK_OFF_OS_INTS_ENABLED);
  uint32_t disable_calls = gc_sdk_state_load_u32be(GC_SDK_OFF_OS_DISABLE_CALLS);
  uint32_t restore_calls = gc_sdk_state_load_u32be(GC_SDK_OFF_OS_RESTORE_CALLS);

  volatile uint8_t *out = (volatile uint8_t *)0x80300000u;
  store_u32be_ptr(out + 0x00, 0xDEADBEEFu);
  store_u32be_ptr(out + 0x04, (uint32_t)outer);
  store_u32be_ptr(out + 0x08, (uint32_t)inner);
  store_u32be_ptr(out + 0x0C, (uint32_t)prev1);
  store_u32be_ptr(out + 0x10, (uint32_t)prev2);
  store_u32be_ptr(out + 0x14, enabled);
  store_u32be_ptr(out + 0x18, disable_calls);
  store_u32be_ptr(out + 0x1C, restore_calls);

  while (1) {
    __asm__ volatile("nop");
  }
  return 0;
}

