#include <stdint.h>

#include "src/sdk_port/gc_mem.c"
#include "src/sdk_port/sdk_state.h"

#include "src/sdk_port/pad/PAD.c"

static inline void store_u32be(volatile uint8_t *p, uint32_t v) {
  p[0] = (uint8_t)(v >> 24);
  p[1] = (uint8_t)(v >> 16);
  p[2] = (uint8_t)(v >> 8);
  p[3] = (uint8_t)(v >> 0);
}

int main(void) {
  gc_mem_set(0x80000000u, 0x01800000u, (uint8_t *)0x80000000u);
  gc_sdk_state_reset();

  // MP4 uses chanTbl bits; we just exercise a non-trivial mask.
  uint32_t mask = 0xA0000000u;
  uint32_t ok = (uint32_t)PADReset(mask);

  uint32_t calls = gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_RESET_CALLS);
  uint32_t got_mask = gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_RESET_MASK);

  volatile uint8_t *out = (volatile uint8_t *)0x80300000u;
  store_u32be(out + 0x00, 0xDEADBEEFu);
  store_u32be(out + 0x04, ok);
  store_u32be(out + 0x08, calls);
  store_u32be(out + 0x0C, got_mask);

  while (1) {
    __asm__ volatile("nop");
  }
  return 0;
}
