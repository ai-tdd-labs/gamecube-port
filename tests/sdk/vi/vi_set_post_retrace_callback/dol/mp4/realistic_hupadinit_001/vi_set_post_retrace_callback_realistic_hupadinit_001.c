#include <stdint.h>

#include "src/sdk_port/gc_mem.c"
#include "src/sdk_port/sdk_state.h"

// Dependencies: MP4 wraps the callback swap in OSDisable/Restore; VI also
// protects the swap internally.
#include "src/sdk_port/os/OSInterrupts.c"
#include "src/sdk_port/vi/VI.c"

static inline void store_u32be_ptr(volatile uint8_t *p, uint32_t v) {
  p[0] = (uint8_t)(v >> 24);
  p[1] = (uint8_t)(v >> 16);
  p[2] = (uint8_t)(v >> 8);
  p[3] = (uint8_t)(v >> 0);
}

// Use fixed "pointer tokens" so PPC-vs-host dumps can match.
// Note: callback invocation is modeled by VIWaitForRetrace (see VI.c).
#define CB_A ((VIRetraceCallback)(uintptr_t)0x80001234u)
#define CB_B ((VIRetraceCallback)(uintptr_t)0x80005678u)

int main(void) {
  gc_mem_set(0x80000000u, 0x01800000u, (uint8_t *)0x80000000u);
  gc_sdk_state_reset();

  // Mirror MP4 HuPadInit structure: disable -> set cb -> restore.
  int lvl = OSDisableInterrupts();
  void *prev0 = (void *)VISetPostRetraceCallback(CB_A);
  OSRestoreInterrupts(lvl);

  // Second set: verify "return previous" behavior.
  lvl = OSDisableInterrupts();
  void *prev1 = (void *)VISetPostRetraceCallback(CB_B);
  OSRestoreInterrupts(lvl);

  uint32_t cb_ptr = gc_sdk_state_load_u32be(GC_SDK_OFF_VI_POST_CB_PTR);
  uint32_t set_calls = gc_sdk_state_load_u32be(GC_SDK_OFF_VI_POST_CB_SET_CALLS);
  uint32_t os_disable_calls = gc_sdk_state_load_u32be(GC_SDK_OFF_OS_DISABLE_CALLS);
  uint32_t os_restore_calls = gc_sdk_state_load_u32be(GC_SDK_OFF_OS_RESTORE_CALLS);

  volatile uint8_t *out = (volatile uint8_t *)0x80300000u;
  store_u32be_ptr(out + 0x00, 0xDEADBEEFu);
  store_u32be_ptr(out + 0x04, (uint32_t)(uintptr_t)prev0);
  store_u32be_ptr(out + 0x08, (uint32_t)(uintptr_t)prev1);
  store_u32be_ptr(out + 0x0C, cb_ptr);
  store_u32be_ptr(out + 0x10, set_calls);
  store_u32be_ptr(out + 0x14, os_disable_calls);
  store_u32be_ptr(out + 0x18, os_restore_calls);

  while (1) {
    __asm__ volatile("nop");
  }
  return 0;
}
