#include <stdint.h>

#include "src/sdk_port/gc_mem.c"
#include "src/sdk_port/sdk_state.h"

#include "src/sdk_port/os/OSInterrupts.c"
#include "src/sdk_port/vi/VI.c"

static inline void store_u32be_ptr(volatile uint8_t *p, uint32_t v) {
  p[0] = (uint8_t)(v >> 24);
  p[1] = (uint8_t)(v >> 16);
  p[2] = (uint8_t)(v >> 8);
  p[3] = (uint8_t)(v >> 0);
}

static volatile uint32_t s_cb_calls;
static volatile uint32_t s_cb_last;

static void PostCB(uint32_t retrace) {
  s_cb_calls++;
  s_cb_last = retrace;
}

int main(void) {
  gc_mem_set(0x80000000u, 0x01800000u, (uint8_t *)0x80000000u);
  gc_sdk_state_reset();

  (void)VISetPostRetraceCallback(PostCB);

  // This should model one retrace and invoke PostCB once.
  VIWaitForRetrace();

  volatile uint8_t *out = (volatile uint8_t *)0x80300000u;
  store_u32be_ptr(out + 0x00, 0xDEADBEEFu);
  store_u32be_ptr(out + 0x04, s_cb_calls);
  store_u32be_ptr(out + 0x08, s_cb_last);
  store_u32be_ptr(out + 0x0C, gc_sdk_state_load_u32be(GC_SDK_OFF_VI_POST_CB_CALLS));
  store_u32be_ptr(out + 0x10, gc_sdk_state_load_u32be(GC_SDK_OFF_VI_POST_CB_LAST_ARG));
  store_u32be_ptr(out + 0x14, gc_sdk_state_load_u32be(GC_SDK_OFF_VI_RETRACE_COUNT));

  while (1) {
    __asm__ volatile("nop");
  }
  return 0;
}

