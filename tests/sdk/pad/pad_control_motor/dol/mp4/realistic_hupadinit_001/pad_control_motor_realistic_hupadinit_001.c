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
  PAD_MOTOR_STOP_HARD = 2,
  PAD_MOTOR_RUMBLE = 1,
  PAD_MOTOR_STOP = 0,
};

int main(void) {
  gc_mem_set(0x80000000u, 0x01800000u, (uint8_t *)0x80000000u);
  gc_sdk_state_reset();

  // Mirror "early init does a bit of motor fiddling".
  PADControlMotor(0, PAD_MOTOR_STOP_HARD);
  PADControlMotor(1, PAD_MOTOR_RUMBLE);
  PADControlMotor(2, PAD_MOTOR_STOP);
  PADControlMotor(3, PAD_MOTOR_STOP_HARD);

  uint32_t c0 = gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_MOTOR_CMD_BASE + 0u);
  uint32_t c1 = gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_MOTOR_CMD_BASE + 4u);
  uint32_t c2 = gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_MOTOR_CMD_BASE + 8u);
  uint32_t c3 = gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_MOTOR_CMD_BASE + 12u);

  volatile uint8_t *out = (volatile uint8_t *)0x80300000u;
  store_u32be_ptr(out + 0x00, 0xDEADBEEFu);
  store_u32be_ptr(out + 0x04, c0);
  store_u32be_ptr(out + 0x08, c1);
  store_u32be_ptr(out + 0x0C, c2);
  store_u32be_ptr(out + 0x10, c3);

  while (1) {
    __asm__ volatile("nop");
  }
  return 0;
}

