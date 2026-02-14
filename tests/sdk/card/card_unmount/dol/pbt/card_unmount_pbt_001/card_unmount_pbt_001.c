#include <stdint.h>

typedef uint8_t u8;
typedef uint32_t u32;

void oracle_run_unmount_suite(volatile u8* out);

static inline volatile u8* out_ptr(void) {
  return (volatile u8*)(uintptr_t)0x80300000u;
}

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;

  volatile u8* out = out_ptr();
  // Clear a deterministic window.
  for (u32 i = 0; i < 0x100u; i++) out[i] = 0;

  oracle_run_unmount_suite(out);
  // Keep Dolphin running until the host grabs the RAM dump via the GDB stub.
  while (1) {
    __asm__ volatile("nop");
  }
}
