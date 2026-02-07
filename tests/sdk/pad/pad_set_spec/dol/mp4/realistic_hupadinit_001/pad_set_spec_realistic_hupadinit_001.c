#include <stdint.h>

#include "src/sdk_port/gc_mem.c"
#include "src/sdk_port/sdk_state.h"

#include "src/sdk_port/pad/PAD.c"

static inline void store_u32be_ptr(volatile uint8_t *p, uint32_t v) {
  p[0] = (uint8_t)(v >> 24);
  p[1] = (uint8_t)(v >> 16);
  p[2] = (uint8_t)(v >> 8);
  p[3] = (uint8_t)(v >> 0);
}

enum {
  PAD_SPEC_5 = 5,
  EXPECT_KIND_SPEC2 = 2,
};

int main(void) {
  gc_mem_set(0x80000000u, 0x01800000u, (uint8_t *)0x80000000u);
  gc_sdk_state_reset();

  // MP4 HuPadInit: PADSetSpec(PAD_SPEC_5)
  PADSetSpec(PAD_SPEC_5);

  uint32_t spec = gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_SPEC);
  uint32_t kind = gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_MAKE_STATUS_KIND);

  volatile uint8_t *out = (volatile uint8_t *)0x80300000u;
  store_u32be_ptr(out + 0x00, 0xDEADBEEFu);
  store_u32be_ptr(out + 0x04, spec);
  store_u32be_ptr(out + 0x08, kind);
  store_u32be_ptr(out + 0x0C, EXPECT_KIND_SPEC2);

  while (1) {
    __asm__ volatile("nop");
  }
  return 0;
}
