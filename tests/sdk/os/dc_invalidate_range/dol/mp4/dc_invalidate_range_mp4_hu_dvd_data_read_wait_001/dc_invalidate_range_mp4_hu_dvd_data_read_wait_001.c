#include <stdint.h>

typedef uint32_t u32;

// MP4 callsite reference:
//   decomp_mario_party_4/src/game/dvd.c: HuDvdDataReadWait()
//   DCInvalidateRange(buf, OSRoundUp32B(len))

#ifndef GC_HOST_TEST
static u32 s_last_addr;
static u32 s_last_len;
static void DCInvalidateRange(void *addr, u32 nbytes) {
  s_last_addr = (u32)(uintptr_t)addr;
  s_last_len = nbytes;
}
#else
extern u32 gc_dc_inval_last_addr;
extern u32 gc_dc_inval_last_len;
void DCInvalidateRange(void *addr, u32 nbytes);
#endif

static volatile u32 *OUT = (u32 *)0x80300000u;

int main(void) {
  // Use a stable address and a rounded length, matching the shape of the callsite.
  void *buf = (void *)0x80400000u;
  u32 len = 0x40;

  DCInvalidateRange(buf, len);

  OUT[0] = 0xDEADBEEFu;
#ifndef GC_HOST_TEST
  OUT[1] = s_last_addr;
  OUT[2] = s_last_len;
#else
  OUT[1] = gc_dc_inval_last_addr;
  OUT[2] = gc_dc_inval_last_len;
#endif

  for (;;) {}
  return 0;
}

