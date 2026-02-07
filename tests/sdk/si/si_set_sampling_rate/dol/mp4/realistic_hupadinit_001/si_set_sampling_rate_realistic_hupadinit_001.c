#include <stdint.h>

#include "src/sdk_port/sdk_state.h"

void VIInit(void);
void SISetSamplingRate(uint32_t msec);

static inline void store_u32be_ptr(volatile uint8_t *p, uint32_t v) {
  p[0] = (uint8_t)(v >> 24);
  p[1] = (uint8_t)(v >> 16);
  p[2] = (uint8_t)(v >> 8);
  p[3] = (uint8_t)(v >> 0);
}

int main(void) {
  gc_mem_set(0x80000000u, 0x01800000u, (uint8_t *)0x80000000u);
  gc_sdk_state_reset();

  VIInit();

  // Mirror MP4 HuPadInit callsite: SISetSamplingRate(0).
  // Keep VIRegs[54] bit0 clear (factor = 1).
  gc_sdk_state_store_u16be_mirror(GC_SDK_OFF_VI_REGS_U16BE + (54u * 2u), 0, 0);

  SISetSamplingRate(0);

  uint32_t rate = gc_sdk_state_load_u32be(GC_SDK_OFF_SI_SAMPLING_RATE);
  uint32_t line = gc_sdk_state_load_u32be(GC_SDK_OFF_SI_SETXY_LINE);
  uint32_t count = gc_sdk_state_load_u32be(GC_SDK_OFF_SI_SETXY_COUNT);
  uint32_t setxy_calls = gc_sdk_state_load_u32be(GC_SDK_OFF_SI_SETXY_CALLS);

  uint32_t ints_enabled = gc_sdk_state_load_u32be(GC_SDK_OFF_OS_INTS_ENABLED);
  uint32_t disable_calls = gc_sdk_state_load_u32be(GC_SDK_OFF_OS_DISABLE_CALLS);
  uint32_t restore_calls = gc_sdk_state_load_u32be(GC_SDK_OFF_OS_RESTORE_CALLS);

  volatile uint8_t *out = (volatile uint8_t *)0x80300000u;
  store_u32be_ptr(out + 0x00, 0xDEADBEEFu);
  store_u32be_ptr(out + 0x04, rate);
  store_u32be_ptr(out + 0x08, line);
  store_u32be_ptr(out + 0x0C, count);
  store_u32be_ptr(out + 0x10, setxy_calls);
  store_u32be_ptr(out + 0x14, ints_enabled);
  store_u32be_ptr(out + 0x18, disable_calls);
  store_u32be_ptr(out + 0x1C, restore_calls);

  while (1) {
    __asm__ volatile("nop");
  }
  return 0;
}
